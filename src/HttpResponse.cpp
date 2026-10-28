
#include "HttpResponse.hpp"

#include <string>

std::string HttpResponse::generate(
    const std::string& body,
    int statusCode,
    const std::string& contentType
) {
    std::string status;

    switch (statusCode) {
        case 200: status = "200 OK"; break;
        case 201: status = "201 Created"; break;
        case 400: status = "400 Bad Request"; break;
        case 404: status = "404 Not Found"; break;
        case 405: status = "405 Method Not Allowed"; break;
        case 413: status = "413 Content Too Large"; break;
        case 500: status = "500 Internal Server Error"; break;
        case 502: status = "502 Bad Gateway"; break;
        case 503: status = "503 Service Unavailable"; break;
        default: status = "500 Internal Server Error"; break;
    }

    return "HTTP/1.1 " + status + "\r\n"
           "Content-Type: " + contentType + "\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Connection: close\r\n"
           "\r\n" + body;
}
