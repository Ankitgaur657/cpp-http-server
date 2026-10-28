
#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <unordered_map>

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string version;
    std::string body;

    std::unordered_map<std::string, std::string> headers;

    bool parse(const std::string& request);
};

#endif
