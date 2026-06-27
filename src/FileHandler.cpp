#include "FileHandler.hpp"

#include <fstream>


string FileHandler::readFile(string path)
{

    ifstream file(path);


    if(!file.is_open())
    {
        return "";
    }


    string content(
        (istreambuf_iterator<char>(file)),
        istreambuf_iterator<char>()
    );


    file.close();


    return content;

}