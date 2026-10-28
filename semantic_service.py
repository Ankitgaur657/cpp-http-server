
import json
import math
import uuid
from functools import lru_cache

import redis
from fastapi import FastAPI, HTTPException
from fastembed import TextEmbedding
from pydantic import BaseModel, Field

app = FastAPI(title="Semantic Cache Service")

REDIS_PREFIX = "semantic_cache:item:"
CACHE_TTL_SECONDS = 86400
SIMILARITY_THRESHOLD = 0.88

redis_client = redis.Redis(
    host="127.0.0.1",
    port=6379,
    decode_responses=True,
    socket_connect_timeout=3,
)

@lru_cache(maxsize=1)
def get_embedder():
    # The model downloads the first time this is initialized.
    return TextEmbedding(model_name="BAAI/bge-small-en-v1.5")


def embed_text(text: str) -> list[float]:
    return list(get_embedder().embed([text]))[0].tolist()


def cosine_similarity(a: list[float], b: list[float]) -> float:
    dot = sum(x * y for x, y in zip(a, b))
    norm_a = math.sqrt(sum(x * x for x in a))
    norm_b = math.sqrt(sum(y * y for y in b))

    if norm_a == 0 or norm_b == 0:
        return 0.0

    return dot / (norm_a * norm_b)


class PromptRequest(BaseModel):
    prompt: str = Field(min_length=1, max_length=10000)


class StoreRequest(BaseModel):
    prompt: str = Field(min_length=1, max_length=10000)
    response: str = Field(min_length=1, max_length=50000)


@app.get("/health")
def health():
    try:
        redis_client.ping()
        return {"status": "ok", "redis": "connected"}
    except redis.RedisError:
        raise HTTPException(status_code=503, detail="Redis unavailable")


@app.post("/lookup")
def lookup(request: PromptRequest):
    prompt = request.prompt.strip()

    if not prompt:
        raise HTTPException(status_code=400, detail="Prompt cannot be empty")

    try:
        query_embedding = embed_text(prompt)

        best_similarity = -1.0
        best_response = None

        # Prototype implementation: compare against stored embeddings.
        for key in redis_client.scan_iter(
            match=f"{REDIS_PREFIX}*",
            count=100,
        ):
            raw = redis_client.get(key)

            if not raw:
                continue

            try:
                item = json.loads(raw)
                score = cosine_similarity(
                    query_embedding,
                    item["embedding"],
                )
            except (ValueError, KeyError, TypeError):
                continue

            if score > best_similarity:
                best_similarity = score
                best_response = item["response"]

        if (
            best_response is not None
            and best_similarity >= SIMILARITY_THRESHOLD
        ):
            return {
                "hit": True,
                "response": best_response,
                "similarity": round(best_similarity, 4),
                "cache_type": "semantic",
            }

        return {
            "hit": False,
            "response": None,
            "similarity": (
                round(best_similarity, 4)
                if best_similarity >= 0
                else None
            ),
            "cache_type": "none",
        }

    except redis.RedisError:
        raise HTTPException(status_code=503, detail="Redis unavailable")
    except Exception as exc:
        raise HTTPException(
            status_code=500,
            detail=f"Semantic lookup failed: {type(exc).__name__}",
        )


@app.post("/store")
def store(request: StoreRequest):
    prompt = request.prompt.strip()

    if not prompt or not request.response.strip():
        raise HTTPException(
            status_code=400,
            detail="Prompt and response cannot be empty",
        )

    try:
        embedding = embed_text(prompt)

        item = {
            "prompt": prompt,
            "response": request.response,
            "embedding": embedding,
        }

        key = f"{REDIS_PREFIX}{uuid.uuid4().hex}"

        redis_client.set(
            key,
            json.dumps(item),
            ex=CACHE_TTL_SECONDS,
        )

        return {
            "stored": True,
            "expires_in_seconds": CACHE_TTL_SECONDS,
        }

    except redis.RedisError:
        raise HTTPException(status_code=503, detail="Redis unavailable")
    except Exception as exc:
        raise HTTPException(
            status_code=500,
            detail=f"Semantic store failed: {type(exc).__name__}",
        )
