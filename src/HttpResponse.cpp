#include "HttpResponse.hpp"



string HttpResponse::generate(
    string body,
    int statusCode
)
{


    string status;



    if(statusCode == 200)
        status = "200 OK";


    else if(statusCode == 404)
        status = "404 Not Found";


    else
        status = "500 Internal Server Error";



    string response =

    "HTTP/1.1 " 
    + status +
    "\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    +
    body;



    return response;


}