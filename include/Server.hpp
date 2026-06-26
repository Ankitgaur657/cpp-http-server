#ifndef SERVER_HPP
#define SERVER_HPP


#include "Socket.hpp"
#include "ThreadPool.hpp"


class Server
{

private:

    Socket socket;

    ThreadPool pool;


public:

    Server(int threads);


    void start();


    void handleClient(int clientSocket);


};


#endif