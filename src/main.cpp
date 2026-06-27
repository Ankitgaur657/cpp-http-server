#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
using namespace std;

#include "Server.hpp"




int main()
{

    Server server(4);


    server.start();


    return 0;

}