#ifndef SYSUTIL_H
#define SYSUTIL_H

#include "common.h"
#include <iostream>
#include <string>
#include "protobuf/filesync.pb.h"
#include <QString>
#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QDebug>
/*系统工具*/
namespace sysutil{
    int read_timeout(int fd,unsigned int wait_seconds);
    int write_timeout(int fd, unsigned int wait_seconds);

    ssize_t readn(int fd, void *buf, size_t count);
    ssize_t writen(int fd, const void *buf, size_t count);
    ssize_t recv_peek(int sockfd, void *buf, size_t len);
    ssize_t readline(int sockfd, void *buf, size_t maxline);

    void fileSend(int desSocket, const char *filename);
    void fileRecv(int recvSocket, char *filename, int size);
    //从应用层缓冲区读取文件数据，append到文件后面
    void fileRecvfromBuf(const char *filename, const char *buf, int size);
    //先发送fileInfo信息再发送文件内容
    void sendfileWithproto(int sockfd, const char *localname, const char *remotename);
    //发送syncInfo信息
    void send_SyncInfo(int socketfd, int id, std::__cxx11::string rootDir, std::string filename,
                                        std::string newname = "");
    void make_Init(int socketfd,std::string root,std::string dir,filesync::Init *msg_init);
    //同步整个文件夹
    void sync_Dir(int socketfd, std::string root, std::string dir);
    //利用qt库获取md5，支持大文件
    std::string getFileMd5(std::string filePath);
}
#endif // SYSUTIL_H
