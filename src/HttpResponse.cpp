#include "../include/HttpResponse.hpp"

string HttpResponse::generate()
{

    string response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<html>"
    "<body>"
    "<h1>Hello from C++ Server</h1>"
    "</body>"
    "</html>";


    return response;

}