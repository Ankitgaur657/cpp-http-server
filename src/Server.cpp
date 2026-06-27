#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "FileHandler.hpp"
#include "Router.hpp"
#include "Logger.hpp"

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>


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
       
    Logger::log("Client handled by thread: "+to_string(hash<thread::id>{}(this_thread::get_id())));

    char buffer[30000] = {0};



    recv(
        clientSocket,
        buffer,
        sizeof(buffer),
        0
    );

      this_thread::sleep_for(
    chrono::seconds(2)
);

    string request(buffer);



    HttpRequest httpRequest;


    httpRequest.parse(request);



   Logger::log(
    "Method: " + httpRequest.method);


    Logger::log(
    "Path: " + httpRequest.path);



    /*
        File handling layer

        Request:

        GET /index.html

        becomes:

        public/index.html
    */


    Router router;


      string filePath =
    router.resolve(httpRequest.path);




    FileHandler fileHandler;



    string content =
fileHandler.readFile(filePath);


int statusCode = 200;



if(content.empty())
{

content =
"<html>"
"<body>"
"<h1>404 Not Found</h1>"
"</body>"
"</html>";


statusCode = 404;

}




   




   HttpResponse response;


string data =
    response.generate(content, statusCode);



send(
    clientSocket,
    data.c_str(),
    data.size(),
    0
);



    close(clientSocket);


}