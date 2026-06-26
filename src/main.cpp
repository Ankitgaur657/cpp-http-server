#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
using namespace std;


int main()
{

    cout << "Starting server..." << endl;


    // Create socket
    int serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );


    if(serverSocket == -1)
    {
        cout<<"Socket creation failed\n";
        return 1;
    }


    cout<<"Socket created successfully\n";

    // Server address structure
    sockaddr_in serverAddress;


    serverAddress.sin_family = AF_INET;

    // Accept connections from any IP
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Port number
    serverAddress.sin_port = htons(8080);



    // Bind socket to address + port
    int bindStatus = bind(
        serverSocket,
        (struct sockaddr*)&serverAddress,
        sizeof(serverAddress)
    );


    if(bindStatus == -1)
    {
        cout << "Binding failed" << endl;
        return 1;
    }


    cout << "Server binded to port 8080" << endl;

    //listening
    int listenStatus=listen(serverSocket,5);
     if(listenStatus==-1){
         cout<<"listening failed\n";
     }
     cout<<"server is listening...\n";


     //accepting
      sockaddr_in clientAddress;
      socklen_t clientSize=sizeof(clientAddress);

      int clientSocket= accept(serverSocket, (struct sockaddr*)&clientAddress,
    &clientSize);
       if(clientSocket==-1){
        cout<<"Accept failed\n";
        return 1;
       }
       cout<<"Client accepted\n";

       //recieving in memory

       char buffer[300000]={0};
       int bytesRecieved=recv(clientSocket,buffer,sizeof(buffer),0);
       if(bytesRecieved==-1){
        cout<<"recieved failed\n";
       }else{
        cout<<"request recieved..\n";
        cout<<buffer<<endl;
       }
       //sending

        string response =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<html>"
"<body>"
"<h1>Hello from C++ Server</h1>"
"</body>"
"</html>";


send(
    clientSocket,
    response.c_str(),
    response.size(),
    0
);


cout << "Response sent" << endl;





     close(clientSocket);
    close(serverSocket);

    return 0;
}