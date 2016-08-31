#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "../common.h"
/* 服务器程序监听的端口号 */
#define PORT 4000
/* 我们一次所能够接收的最大字节数 */
#define MAXDATASIZE 100
int main()
{
    char *argv[2] = {0,"127.0.0.1"};
    /* 套接字描述符 */
    int sockfd,numbytes;
    char buf[MAXDATASIZE];
    struct hostent *he;
    /* 连接者的主机信息 */
    struct sockaddr_in their_addr;
    /* 取得主机信息 */
    if ((he=gethostbyname(argv[1])) == NULL)
    {
        /* 如果 gethostbyname()发生错误，则显示错误信息并退出 */
        herror("gethostbyname");
        exit(1);
    }
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        /* 如果 socket()调用出现错误则显示错误信息并退出 */
        perror("socket");
        exit(1);
    }
    /* 主机字节顺序 */
    their_addr.sin_family = AF_INET;
    /* 网络字节顺序，短整型 */
    their_addr.sin_port = htons(PORT);
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    /* 将结构剩下的部分清零*/
    bzero(&(their_addr.sin_zero), 8);
    if(connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1)
    {
        /* 如果 connect()建立连接错误，则显示出错误信息，退出 */
        perror("connect");
        exit(1);
    }

    if (send(sockfd, "5", 1, MSG_OOB)== -1)
    {
        perror("send");
        close(sockfd);
        exit(0);
    }
    printf("Send 1 byte of OOB data\n");
    sleep(2);
    /* 这里就是我们说的错误检查！ */
    if (send(sockfd, "123", 3, 0) == -1)//   123  normal data
    {
        /* 如果错误，则给出错误提示，然后关闭这个新连接，退出 */
        perror("send");
        close(sockfd);
        exit(0);
    }
    printf("Send 3 byte of normal data\n");
    /* 睡眠 1 秒 */
    sleep(2);
    if (send(sockfd, "4", 1, MSG_OOB)== -1)
    {
        perror("send");
        close(sockfd);
        exit(0);
    }
    printf("Send 1 byte of OOB data\n");
    sleep(2);
    if (send(sockfd, "56", 2, 0) == -1)//  56  normal data
    {
        perror("send");
        close(sockfd);
        exit(0);
    }
    printf("Send 2 bytes of normal data\n");
    sleep(2);
    if (send(sockfd, "7", 1, MSG_OOB)== -1)
    {
        perror("send");
        close(sockfd);
        exit(0);
    }
    printf("Send 1 byte of OOB data\n");
    sleep(2);
    if (send(sockfd, "89", 2, MSG_OOB)== -1)
    {
        perror("send");
        close(sockfd);
        exit(0);
    }
    printf("Send 2 bytes of OOB data\n");
    sleep(2);
    /*
 * if (send(sockfd, "ABC", 3, MSG_OOB)== -1)
 * {
 * perror("send");
 * close(sockfd);
 * exit(0);
 * }
 * printf("Send 3 bytes of OOB data\n");
 * sleep(2);
 * if (send(sockfd, "abc", 3, MSG_OOB)== -1)
 * {
 * perror("send");
 * close(sockfd);
 * exit(0);
 * }
 * printf("Send 3 bytes of OOB data\n");
 * sleep(2);
 * */
    close(sockfd);
    return 0;
}

