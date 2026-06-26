#include "../include/HttpRequest.hpp"

#include <sstream>


void HttpRequest::parse(string request)
{

    stringstream stream(request);


    stream >> method;

    stream >> path;


}