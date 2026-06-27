#include "Logger.hpp"


mutex Logger::logMutex;



void Logger::log(string message)
{

    lock_guard<mutex> lock(logMutex);


    cout << message << endl;

}