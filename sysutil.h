#ifndef SYSUTIL_H
#define SYSUTIL_H

#include "common.h"
#include "mutexlock.h"
#include "eventloop.h"
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
    ssize_t writen(int fd, const void *buf, size_t count);

    //从应用层缓冲区读取文件数据，append到文件后面
    void fileRecvfromBuf(const char *filename,const char *buf,int size);
    //先发送fileInfo信息再发送文件内容
    void sendfileWithproto(int sockfd, const char *localname, const char *remotename, EventLoop *loop);
    //发送syncInfo信息
    void send_SyncInfo(int socketfd, int id, std::__cxx11::string rootDir, std::string filename,
                                        std::string newname = "", int removedSize=-1);
    void make_Init(int socketfd,std::string root,std::string dir,filesync::Init *msg_init);
    //同步整个文件夹
    void sync_Dir(int socketfd, std::string root, std::string dir);
    //利用qt库获取md5，支持大文件
    std::string getFileMd5(std::string filePath);

    std::string getRemovedFilename(string rootDir,string remoteName);
    //把文件名前面的.都去掉
    string getOriginName(string filename);
}
#endif // SYSUTIL_H
