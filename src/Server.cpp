
#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "FileHandler.hpp"
#include "Router.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
#include <shared_mutex>
#include <curl/curl.h>
#include <stdexcept>
#include <mutex>
#include <nlohmann/json.hpp>
#include <unordered_map>

using namespace std;
using json = nlohmann::json;

namespace {
    unordered_map<string, string> exactCache;
    shared_mutex cacheMutex;
}
namespace {
    atomic<unsigned long long> totalRequests{0};
    atomic<unsigned long long> exactCacheHits{0};
    atomic<unsigned long long> semanticCacheHits{0};
    atomic<unsigned long long> cacheMisses{0};
}


static size_t llamaWriteCallback(
    char* data, size_t size, size_t count, void* userData
) {
    size_t bytes = size * count;
    auto* result = static_cast<std::string*>(userData);
    result->append(data, bytes);
    return bytes;
}

  static bool initializeCurl() {
    static const bool initialized =
        curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    return initialized;
}

static std::string queryQwen(const std::string& prompt) {
    using json = nlohmann::json;

 

    if (!initializeCurl()) {
        throw std::runtime_error("curl initialization failed");
    }

    CURL* curl = curl_easy_init();

    if (!curl) {
        throw std::runtime_error("Could not create curl handle");
    }

    json payload = {
        {"model", "qwen-local"},
        {"messages", json::array({
            {
                {"role", "user"},
                {"content", prompt}
            }
        })},
        {"max_tokens", 128},
        {"stream", false}
    };

    std::string requestBody = payload.dump();
    std::string responseBody;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(
        headers, "Content-Type: application/json"
    );

    curl_easy_setopt(
        curl, CURLOPT_URL,
        "http://127.0.0.1:8081/v1/chat/completions"
    );
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(
        curl, CURLOPT_POSTFIELDS, requestBody.c_str()
    );
    curl_easy_setopt(
        curl, CURLOPT_POSTFIELDSIZE,
        static_cast<long>(requestBody.size())
    );
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION, llamaWriteCallback
    );
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode result = curl_easy_perform(curl);

    long statusCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK || statusCode != 200) {
        throw std::runtime_error("Qwen inference request failed");
    }

    json response = json::parse(responseBody);

    if (!response.contains("choices") ||
        !response["choices"].is_array() ||
        response["choices"].empty() ||
        !response["choices"][0].contains("message") ||
        !response["choices"][0]["message"].contains("content")) {
        throw std::runtime_error("Invalid response from Qwen");
    }

    return response["choices"][0]["message"]["content"].get<std::string>();
}



static bool lookupSemanticCache(
    const std::string& prompt,
    std::string& cachedResponse
) {
    if (!initializeCurl()) {
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    json payload = {{"prompt", prompt}};
    std::string requestBody = payload.dump();
    std::string responseBody;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(
        headers, "Content-Type: application/json"
    );

    curl_easy_setopt(
        curl, CURLOPT_URL, "http://127.0.0.1:8090/lookup"
    );
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(
        curl, CURLOPT_POSTFIELDS, requestBody.c_str()
    );
    curl_easy_setopt(
        curl, CURLOPT_POSTFIELDSIZE,
        static_cast<long>(requestBody.size())
    );
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, llamaWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode result = curl_easy_perform(curl);
    long statusCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK || statusCode != 200) {
        Logger::log("Semantic cache lookup unavailable");
        return false;
    }

    try {
        json resultJson = json::parse(responseBody);

        if (resultJson.value("hit", false) &&
            resultJson.contains("response") &&
            resultJson["response"].is_string()) {
            cachedResponse =
                resultJson["response"].get<std::string>();
            return true;
        }
    } catch (const std::exception&) {
        Logger::log("Invalid semantic cache response");
    }

    return false;
}

static void storeSemanticCache(
    const std::string& prompt,
    const std::string& generatedResponse
) {
    if (!initializeCurl()) {
        return;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return;
    }

    json payload = {
        {"prompt", prompt},
        {"response", generatedResponse}
    };

    std::string requestBody = payload.dump();
    std::string responseBody;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(
        headers, "Content-Type: application/json"
    );

    curl_easy_setopt(
        curl, CURLOPT_URL, "http://127.0.0.1:8090/store"
    );
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(
        curl, CURLOPT_POSTFIELDS, requestBody.c_str()
    );
    curl_easy_setopt(
        curl, CURLOPT_POSTFIELDSIZE,
        static_cast<long>(requestBody.size())
    );
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, llamaWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode result = curl_easy_perform(curl);
    long statusCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK || statusCode != 200) {
        Logger::log("Semantic cache store failed");
    }
}




Server::Server(int threads) : pool(threads) {}

void Server::start() {
    socket.bindSocket(8080);
    socket.listenSocket();

    cout << "Server started on port 8080..." << endl;

    while (true) {
        int clientSocket = socket.acceptConnection();

        if (clientSocket < 0) {
            continue;
        }

        pool.enqueue([this, clientSocket]() {
            handleClient(clientSocket);
        });
    }
}

void Server::handleClient(int clientSocket) {
     ++totalRequests;
    Logger::log(
        "Client handled by thread: " +
        to_string(hash<thread::id>{}(this_thread::get_id()))
    );

    string request;
    char buffer[4096];

    // Read until the complete HTTP headers arrive.
    size_t headerEnd = string::npos;

    while (headerEnd == string::npos) {
        ssize_t received = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (received <= 0) {
            close(clientSocket);
            return;
        }

        request.append(buffer, static_cast<size_t>(received));

        if (request.size() > 1024 * 1024) {
            HttpResponse response;
            string data = response.generate("Request too large", 413);
            send(clientSocket, data.c_str(), data.size(), 0);
            close(clientSocket);
            return;
        }

        headerEnd = request.find("\r\n\r\n");
    }

    // Determine how many body bytes are expected.
    size_t contentLength = 0;
    size_t lineStart = request.find("\r\n") + 2;

    while (lineStart < headerEnd) {
        size_t lineEnd = request.find("\r\n", lineStart);

        if (lineEnd == string::npos || lineEnd > headerEnd) {
            break;
        }

        string line = request.substr(lineStart, lineEnd - lineStart);
        size_t colon = line.find(':');

        if (colon != string::npos) {
            string name = line.substr(0, colon);

            transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c) {
                    return static_cast<char>(tolower(c));
                });

            if (name == "content-length") {
                try {
                    size_t parsed = 0;
                    unsigned long long length =
                        stoull(line.substr(colon + 1), &parsed);

                    if (parsed == 0 || length > 1024 * 1024) {
                        HttpResponse response;
                        string data =
                            response.generate("Invalid Content-Length", 400);
                        send(clientSocket, data.c_str(), data.size(), 0);
                        close(clientSocket);
                        return;
                    }

                    contentLength = static_cast<size_t>(length);
                } catch (...) {
                    HttpResponse response;
                    string data =
                        response.generate("Invalid Content-Length", 400);
                    send(clientSocket, data.c_str(), data.size(), 0);
                    close(clientSocket);
                    return;
                }
            }
        }

        lineStart = lineEnd + 2;
    }

    // Read any remaining request body.
    while (request.size() - (headerEnd + 4) < contentLength) {
        ssize_t received = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (received <= 0) {
            close(clientSocket);
            return;
        }

        request.append(buffer, static_cast<size_t>(received));
    }

    HttpRequest httpRequest;

    if (!httpRequest.parse(request)) {
        HttpResponse response;
        string data = response.generate("Bad Request", 400);
        send(clientSocket, data.c_str(), data.size(), 0);
        close(clientSocket);
        return;
    }

    Logger::log("Method: " + httpRequest.method);
    Logger::log("Path: " + httpRequest.path);

     if (httpRequest.path == "/metrics") {
    unsigned long long total = totalRequests.load();
    unsigned long long exactHits = exactCacheHits.load();
    unsigned long long semanticHits = semanticCacheHits.load();
    unsigned long long misses = cacheMisses.load();

    unsigned long long cacheLookups =
        exactHits + semanticHits + misses;

    double hitRate = cacheLookups == 0
        ? 0.0
        : 100.0 * (exactHits + semanticHits) / cacheLookups;

    nlohmann::json metrics = {
        {"total_requests", total},
        {"exact_cache_hits", exactHits},
        {"semantic_cache_hits", semanticHits},
        {"cache_misses", misses},
        {"cache_hit_rate_percent", hitRate}
    };



    HttpResponse response;
    string data = response.generate(
        metrics.dump(2),
        200,
        "application/json"
    );

    send(clientSocket, data.c_str(), data.size(), 0);
    close(clientSocket);
    return;
}

    
    // AI gateway endpoint
    if (httpRequest.path == "/v1/chat") {
        HttpResponse response;

        if (httpRequest.method != "POST") {
            string data = response.generate(
                R"({"error":"Method not allowed"})",
                405,
                "application/json"
            );
            send(clientSocket, data.c_str(), data.size(), 0);
            close(clientSocket);
            return;
        }

        try {
            using json = nlohmann::json;

            json input = json::parse(httpRequest.body);

            if (!input.contains("prompt") ||
                !input["prompt"].is_string() ||
                input["prompt"].get<string>().empty()) {
                string data = response.generate(
                    R"({"error":"A non-empty string prompt is required"})",
                    400,
                    "application/json"
                );
                send(clientSocket, data.c_str(), data.size(), 0);
                close(clientSocket);
                return;
            }

           string prompt = input["prompt"].get<string>();

// Normalize surrounding whitespace for the cache key.
size_t first = prompt.find_first_not_of(" \t\r\n");
size_t last = prompt.find_last_not_of(" \t\r\n");

if (first == string::npos) {
    string data = response.generate(
        R"({"error":"Prompt cannot be empty"})",
        400,
        "application/json"
    );
    send(clientSocket, data.c_str(), data.size(), 0);
    close(clientSocket);
    return;
}

string cacheKey = prompt.substr(first, last - first + 1);

string cachedResponse;
{
    shared_lock<shared_mutex> lock(cacheMutex);
    auto it = exactCache.find(cacheKey);

    if (it != exactCache.end()) {
        cachedResponse = it->second;
    }
}

json output;

if (!cachedResponse.empty()) {
    // 1. Exact cache hit
    ++exactCacheHits;

    output = json::parse(cachedResponse);
    output["cached"] = true;
    output["cache_type"] = "exact";

} else {
    // 2. Semantic cache lookup
    std::string semanticResponse;

    if (lookupSemanticCache(cacheKey, semanticResponse)) {
        ++semanticCacheHits;

        output = {
            {"response", semanticResponse},
            {"cached", true},
            {"cache_type", "semantic"}
        };

    } else {
        // 3. Both caches missed: call Qwen
        ++cacheMisses;

        try {
            std::string generatedResponse = queryQwen(cacheKey);

            output = {
                {"response", generatedResponse},
                {"cached", false},
                {"cache_type", "none"}
            };

            // 4. Save the generated response in semantic cache
            storeSemanticCache(cacheKey, generatedResponse);

        } catch (const std::exception& e) {
            Logger::log(
                std::string("Qwen backend error: ") + e.what()
            );

            string data = response.generate(
                R"({"error":"AI backend unavailable"})",
                502,
                "application/json"
            );

            send(clientSocket, data.c_str(), data.size(), 0);
            close(clientSocket);
            return;
        }
    }

    // Save this prompt's response in the exact cache too.
    {
        unique_lock<shared_mutex> lock(cacheMutex);
        exactCache[cacheKey] = output.dump();
    }
}



            string data = response.generate(
                output.dump(),
                200,
                "application/json"
            );

            send(clientSocket, data.c_str(), data.size(), 0);
            close(clientSocket);
            return;

       } catch (const nlohmann::json::exception&) {
            string data = response.generate(
                R"({"error":"Invalid JSON"})",
                400,
                "application/json"
            );
            send(clientSocket, data.c_str(), data.size(), 0);
            close(clientSocket);
            return;
        }
    }

    // Existing static-file routing
    Router router;
    string filePath = router.resolve(httpRequest.path);


    FileHandler fileHandler;
    string content = fileHandler.readFile(filePath);

    int statusCode = 200;

    if (content.empty()) {
        content = "<html><body><h1>404 Not Found</h1></body></html>";
        statusCode = 404;
    }

    HttpResponse response;
    string data = response.generate(content, statusCode);

    send(clientSocket, data.c_str(), data.size(), 0);
    close(clientSocket);
}
