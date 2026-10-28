
#!/bin/bash

set -u

URL="http://127.0.0.1:8080/v1/chat"
METRICS_URL="http://127.0.0.1:8080/metrics"

REQUESTS="${1:-30}"
CONCURRENCY="${2:-5}"
MODE="${3:-exact}"

if ! [[ "$REQUESTS" =~ ^[1-9][0-9]*$ ]] ||
   ! [[ "$CONCURRENCY" =~ ^[1-9][0-9]*$ ]]; then
    echo "Usage: $0 [requests] [concurrency] [exact|semantic|miss]"
    exit 1
fi

if [[ "$MODE" != "exact" &&
      "$MODE" != "semantic" &&
      "$MODE" != "miss" ]]; then
    echo "Mode must be exact, semantic, or miss."
    exit 1
fi

if ! curl -fsS "$METRICS_URL" >/dev/null; then
    echo "Gateway is not responding at port 8080."
    exit 1
fi

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

RUN_ID=$(date +%s)

echo "Preparing benchmark: mode=$MODE"

# Prepare the relevant cache path before timing.
if [[ "$MODE" == "exact" ]]; then
    PROMPT="Explain binary search in one sentence"

    PAYLOAD=$(python3 -c \
      'import json,sys; print(json.dumps({"prompt":sys.argv[1]}))' \
      "$PROMPT")

    curl -fsS -X POST "$URL" \
      -H "Content-Type: application/json" \
      -d "$PAYLOAD" >/dev/null || {
        echo "Warm-up request failed."
        exit 1
    }

elif [[ "$MODE" == "semantic" ]]; then
    # Seed Redis semantic cache with a known response.
    curl -fsS -X POST http://127.0.0.1:8090/store \
      -H "Content-Type: application/json" \
      -d '{"prompt":"Explain binary search","response":"Binary search repeatedly halves a sorted search range to find a target."}' \
      >/dev/null || {
        echo "Could not seed semantic cache. Is port 8090 running?"
        exit 1
    }
fi

curl -fsS "$METRICS_URL" > "$TMP_DIR/metrics_before.json"

echo "Starting $REQUESTS requests with concurrency $CONCURRENCY..."
START_NS=$(date +%s%N)

for i in $(seq 1 "$REQUESTS"); do
    (
        case "$MODE" in
            exact)
                PROMPT="Explain binary search in one sentence"
                ;;
            semantic)
                PROMPT="Explain how binary search works, benchmark case ${RUN_ID}-${i}"
                ;;
            miss)
                PROMPT="Explain hypothetical subsystem QZX${RUN_ID} item ${i} in a computer architecture"
                ;;
        esac

        PAYLOAD=$(python3 -c \
          'import json,sys; print(json.dumps({"prompt":sys.argv[1]}))' \
          "$PROMPT")

        BODY="$TMP_DIR/body_$i.json"
        RESULT="$TMP_DIR/result_$i.tsv"

        TIME_AND_STATUS=$(curl -sS \
          --connect-timeout 3 \
          --max-time 150 \
          -o "$BODY" \
          -w '%{time_total}\t%{http_code}' \
          -X POST "$URL" \
          -H "Content-Type: application/json" \
          -d "$PAYLOAD")

        CURL_STATUS=$?

        if [[ "$CURL_STATUS" -ne 0 ]]; then
            printf '0\t000\ttransport_error\n' > "$RESULT"
            exit 0
        fi

        CACHE_TYPE=$(python3 -c '
import json,sys
try:
    with open(sys.argv[1]) as f:
        print(json.load(f).get("cache_type", "unknown"))
except Exception:
    print("unknown")
' "$BODY")

        printf '%s\t%s\n' "$TIME_AND_STATUS" \
          "$CACHE_TYPE" > "$RESULT"
    ) &

    # Limit the number of simultaneous curl processes.
    while (( $(jobs -rp | wc -l) >= CONCURRENCY )); do
        wait -n || true
    done
done

wait || true

END_NS=$(date +%s%N)
ELAPSED_MS=$(( (END_NS - START_NS) / 1000000 ))

curl -fsS "$METRICS_URL" > "$TMP_DIR/metrics_after.json"

echo
echo "========== BENCHMARK RESULTS =========="
echo "Mode:             $MODE"
echo "Requests planned: $REQUESTS"
echo "Concurrency:      $CONCURRENCY"
echo "Elapsed time:     ${ELAPSED_MS} ms"

python3 - "$TMP_DIR" "$REQUESTS" "$ELAPSED_MS" <<'PY'
import glob
import json
import os
import sys

directory = sys.argv[1]
planned = int(sys.argv[2])
elapsed_ms = int(sys.argv[3])

results = []
for path in glob.glob(os.path.join(directory, "result_*.tsv")):
    with open(path) as f:
        parts = f.read().strip().split("\t")
    if len(parts) != 3:
        continue
    try:
        latency_ms = float(parts[0]) * 1000
    except ValueError:
        latency_ms = 0.0
    results.append((latency_ms, parts[1], parts[2]))

results.sort(key=lambda r: r[0])

successful = sum(code.startswith("2") for _, code, _ in results)
http_errors = sum(
    code != "000" and not code.startswith("2")
    for _, code, _ in results
)
transport_errors = sum(code == "000" for _, code, _ in results)

def percentile(values, p):
    if not values:
        return 0.0
    index = max(0, int((p / 100) * len(values) + 0.999999) - 1)
    return values[min(index, len(values) - 1)]

latencies = [latency for latency, _, _ in results]

print(f"Requests completed: {len(results)}")
print(f"Successful (2xx):   {successful}")
print(f"HTTP errors:        {http_errors}")
print(f"Transport errors:   {transport_errors}")
print(f"Throughput:         {successful / (elapsed_ms / 1000):.2f} successful req/s"
      if elapsed_ms > 0 else "Throughput:         N/A")
print(f"Average latency:    {sum(latencies) / len(latencies):.2f} ms"
      if latencies else "Average latency:    N/A")
print(f"p50 latency:        {percentile(latencies, 50):.2f} ms")
print(f"p95 latency:        {percentile(latencies, 95):.2f} ms")
print(f"p99 latency:        {percentile(latencies, 99):.2f} ms")

print("\nCache types:")
for name in ("exact", "semantic", "none", "unknown", "transport_error"):
    count = sum(cache == name for _, _, cache in results)
    print(f"  {name:16s} {count}")

before_path = os.path.join(directory, "metrics_before.json")
after_path = os.path.join(directory, "metrics_after.json")

try:
    with open(before_path) as f:
        before = json.load(f)
    with open(after_path) as f:
        after = json.load(f)

    print("\nGateway metric changes:")
    for key in (
        "exact_cache_hits",
        "semantic_cache_hits",
        "cache_misses",
    ):
        print(f"  {key}: {after.get(key, 0) - before.get(key, 0)}")
except Exception:
    print("\nCould not calculate gateway metric changes.")
PY

echo "========================================"
