#ifndef LOGGER_HPP
#define LOGGER_HPP


#include <iostream>
#include <mutex>

using namespace std;


class Logger
{

private:

    static mutex logMutex;


public:

    static void log(string message);

};


#endif