#include "socket.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

TcpSocket::TcpSocket()
{   
    //初始化Socket
    if( (socket_fd = ::socket(AF_INET, SOCK_STREAM, 0)) == -1 )
        CHEN_LOG(ERROR,"create socket error");
}

TcpSocket::~TcpSocket()
{
    ::close(socket_fd);
}
/**
 * @brief TcpSocket::connect
 * @param ip
 * @param port
 * @return 返回成功连接的socket
 */
int TcpSocket::connect(string ip,int port)
{
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr) <= 0)
    {
       CHEN_LOG(ERROR,"inet_pton error");
    }
    servaddr.sin_port = htons(port);//设置的端口为DEFAULT_PORT
    if(::connect(socket_fd,(struct sockaddr*)&servaddr,sizeof(servaddr)) <0 )
        CHEN_LOG(ERROR,"connect error");
    return socket_fd;
}
