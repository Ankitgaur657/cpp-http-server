#include "MimeType.hpp"



string MimeType::get(string path)
{


if(path.find(".css") != string::npos)
    return "text/css";


if(path.find(".js") != string::npos)
    return "application/javascript";


if(path.find(".png") != string::npos)
    return "image/png";


return "text/html";


}