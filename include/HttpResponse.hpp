#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP
#include <string>
using namespace std;


class HttpResponse
{

public:

    string generate(
    string body,
    int statusCode
);


};


#endif