#ifndef SOCKET_H
#define SOCKET_H
#include <iostream>
#include "common.h"
using namespace std;

class TcpSocket
{
public:
    TcpSocket();
    ~TcpSocket();
    int connect(string ip,int port);
    void shutdown();
    int getSocket() {   return socket_fd;   }
    void setKeepalive();
private:
    struct sockaddr_in servaddr;
    int socket_fd;
    string ip;  //对方的ip和端口
    int port;


};

#endif // SOCKET_H
