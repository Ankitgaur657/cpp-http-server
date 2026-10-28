
#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>

class HttpResponse {
public:
    std::string generate(
        const std::string& body,
        int statusCode,
        const std::string& contentType = "text/html; charset=utf-8"
    );
};

#endif
