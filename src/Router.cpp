#include "Router.hpp"



string Router::resolve(string path)
{


    if(path == "/")
    {
        return "public/index.html";
    }


    return "public" + path;


}