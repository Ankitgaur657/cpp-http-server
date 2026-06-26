#ifndef SOCKET_HPP
#define SOCKET_HPP


class Socket
{

private:

    int fd;


public:

    Socket();

    void bindSocket(int port);

    void listenSocket();

    int acceptConnection();


};


#endif