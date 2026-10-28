
#include "HttpRequest.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string lowercase(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    return value;
}
}

bool HttpRequest::parse(const std::string& request) {
    method.clear();
    path.clear();
    version.clear();
    body.clear();
    headers.clear();

    const auto headerEnd = request.find("\r\n\r\n");

    if (headerEnd == std::string::npos) {
        return false;
    }

    std::istringstream stream(request.substr(0, headerEnd));
    std::string line;

    if (!std::getline(stream, line)) {
        return false;
    }

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    std::istringstream requestLine(line);

    if (!(requestLine >> method >> path >> version)) {
        return false;
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        return false;
    }

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const auto colon = line.find(':');

        if (colon == std::string::npos || colon == 0) {
            return false;
        }

        const std::string name =
            lowercase(trim(line.substr(0, colon)));

        const std::string value =
            trim(line.substr(colon + 1));

        headers[name] = value;
    }

    body = request.substr(headerEnd + 4);

    const auto lengthIt = headers.find("content-length");

    if (lengthIt != headers.end()) {
        try {
            std::size_t consumed = 0;
            const auto contentLength =
                std::stoull(lengthIt->second, &consumed);

            if (consumed != lengthIt->second.size()) {
                return false;
            }

            if (contentLength != body.size()) {
                return false;
            }
        } catch (...) {
            return false;
        }
    }

    return true;
}
