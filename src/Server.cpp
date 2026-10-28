
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
    atomic<unsigned long long> cacheMisses{0};
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
    unsigned long long hits = exactCacheHits.load();
    unsigned long long misses = cacheMisses.load();

    double hitRate = (hits + misses) == 0
        ? 0.0
        : 100.0 * hits / (hits + misses);

    nlohmann::json metrics = {
        {"total_requests", total},
        {"exact_cache_hits", hits},
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

bool cacheHit = !cachedResponse.empty();
json output;

if (cacheHit) {
    ++exactCacheHits;
    output = json::parse(cachedResponse);
    output["cached"] = true;
    output["cache_type"] = "exact";
} else {
    ++cacheMisses;
    output = {
        {"response", "Mock AI response for: " + cacheKey},
        {"cached", false},
        {"cache_type", "none"}
    };

    // Save the original response for future requests.
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
