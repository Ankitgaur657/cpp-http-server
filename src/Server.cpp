#include "../include/Server.hpp"

#include "../include/HttpRequest.hpp"
#include "../include/HttpResponse.hpp"


#include <iostream>

#include <sys/socket.h>
#include <unistd.h>


using namespace std;



Server::Server(int threads)
    :
    pool(threads)
{

}



void Server::start()
{


    socket.bindSocket(8080);


    socket.listenSocket();



    cout<<"Server started..."<<endl;



    while(true)
    {


        int clientSocket =
        socket.acceptConnection();



        cout<<"New client"<<endl;



        pool.enqueue(
        
        [this, clientSocket]()
        {

            handleClient(clientSocket);

        }

        );


    }



}



void Server::handleClient(int clientSocket)
{


    char buffer[30000] = {0};



    recv(
        clientSocket,
        buffer,
        sizeof(buffer),
        0
    );



    string request(buffer);



    HttpRequest httpRequest;


    httpRequest.parse(request);



    cout<<"Method: "
        <<httpRequest.method
        <<endl;



    HttpResponse response;



    string data =
    response.generate();



    send(
        clientSocket,
        data.c_str(),
        data.size(),
        0
    );



    close(clientSocket);



}