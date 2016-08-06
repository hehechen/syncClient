#include "socket.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>

TcpSocket::TcpSocket()
{   
    //初始化Socket
    if( (socket_fd = ::socket(AF_INET, SOCK_STREAM, 0)) == -1 )
        CHEN_LOG(ERROR,"create socket error");
}

TcpSocket::~TcpSocket()
{
    //  ::close(socket_fd);
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

void TcpSocket::setKeepalive()
{
    int keepalive = 1; // 开启keepalive属性
    int keepidle = 30; // 如该连接在30秒内没有任何数据往来,则进行探测
    int keepinterval = 2; // 探测时发包的时间间隔为2 秒
    int keepcount = 3; // 探测尝试的次数.如果第1次探测包就收到响应了,则后2次的不再发.
    int ret = setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive ));
    ret = setsockopt(socket_fd, SOL_TCP, TCP_KEEPIDLE, (void*)&keepidle , sizeof(keepidle ));
    ret = setsockopt(socket_fd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepinterval , sizeof(keepinterval ));
    ret = setsockopt(socket_fd, SOL_TCP, TCP_KEEPCNT, (void *)&keepcount , sizeof(keepcount ));
    if(ret<0)
        CHEN_LOG(ERROR,"set keepalive error");
}
