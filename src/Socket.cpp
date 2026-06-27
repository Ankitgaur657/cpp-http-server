#include "Socket.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
using namespace std;



Socket::Socket()
{

    fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );


    if(fd == -1)
    {
        cout<<"Socket creation failed"<<endl;
    }


}



void Socket::bindSocket(int port)
{

    sockaddr_in serverAddress;


    serverAddress.sin_family = AF_INET;

    serverAddress.sin_addr.s_addr = INADDR_ANY;

    serverAddress.sin_port = htons(port);



    int status = bind(
        fd,
        (struct sockaddr*)&serverAddress,
        sizeof(serverAddress)
    );


    if(status == -1)
    {
        cout<<"Bind failed"<<endl;
    }


}



void Socket::listenSocket()
{

    listen(fd,5);

    cout<<"Listening..."<<endl;

}




int Socket::acceptConnection()
{

    sockaddr_in clientAddress;

    socklen_t size = sizeof(clientAddress);



    int client =
        accept(
            fd,
            (struct sockaddr*)&clientAddress,
            &size
        );


    return client;

}