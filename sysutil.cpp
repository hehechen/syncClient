#include "sysutil.h"
#include "protobuf/filesync.pb.h"
#include "codec.h"
#include <iostream>
#include <string>

using namespace std;
namespace sysutil{

MutexLock syncInfoLock;

/**
 * writen - 发送固定字节数
 * @fd: 文件描述符
 * @buf: 发送缓冲区
 * @count: 要读取的字节数
 * 成功返回count，失败返回-1
 */
ssize_t writen(int fd, const void *buf, size_t count)
{
    size_t nleft = count;
    ssize_t nwritten;
    char *bufp = (char*)buf;

    while (nleft > 0)
    {
        if ((nwritten = write(fd, bufp, nleft)) < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (nwritten == 0)
            continue;

        bufp += nwritten;
        nleft -= nwritten;
    }

    return count;
}

//从应用层缓冲区读取文件数据，append到文件后面
void fileRecvfromBuf(const char *filename,const char *buf,int size)
{
    int fd = open(filename, O_CREAT | O_WRONLY, 0666);
    if (fd == -1)
    {
        CHEN_LOG(WARN,"Could not create file %s",filename);
        return;
    }
    lseek(fd,0,SEEK_END);
    if(write(fd,buf,size) < 0)
        CHEN_LOG(ERROR,"write file %s error",filename);
    close(fd);
}

/**
 * @brief sendfileWithproto 先发送fileInfo信息再发送文件内容
 * @param sockfd
 * @param localname 本地路径
 * @param remotename    不包含顶层文件夹的文件名
 * @param loop
 */
void sendfileWithproto(int sockfd,const char* localname,const char *remotename,EventLoop *loop)
{
    int fd = open(localname,O_RDONLY);
    if(-1 == fd)
        CHEN_LOG(WARN,"open file %s error",localname);
    struct stat sbuf;
    int ret = fstat(fd,&sbuf);
    if (!S_ISREG(sbuf.st_mode))
    {//不是一个普通文件
        CHEN_LOG(WARN,"failed to open file");
        return;
    }
    int total_size;
    int bytes_to_send;
    total_size = bytes_to_send = sbuf.st_size; //  文件大小
    //先发送fileInfo信息
    filesync::FileInfo msg;
    msg.set_size(bytes_to_send);
    msg.set_filename(string(remotename));
    string send_msg = Codec::enCode(msg);
    if(writen(sockfd,send_msg.c_str(),send_msg.size()) == -1)
        CHEN_LOG(ERROR,"failed to write socket");
    CHEN_LOG(DEBUG,"file size:%d=======filename:%s"
                   "========message size:%d",bytes_to_send,localname,send_msg.size());
    //开始传输
    while(bytes_to_send)
    {
        if(loop->isRemoved(pthread_self(),localname))
        {
            send_SyncInfo(loop->getControlSocket(),3,loop->getRootDir(),
                          localname,"",total_size-bytes_to_send);
            CHEN_LOG(INFO,"SEND SIZE:%d",total_size-bytes_to_send);
            return;
        }

        int num_this_time = bytes_to_send > 4096 ? 4096:bytes_to_send;
        ret = sendfile(sockfd,fd,NULL,num_this_time);//不会返回EINTR,ret是发送成功的字节数
        if (-1 == ret)
        {
            //发送失败
            CHEN_LOG(ERROR,"send file failed");
            break;
        }
        bytes_to_send -= ret;//更新剩下的字节数
    }
    CHEN_LOG(DEBUG,"sendfile%s  COMPLETED",localname);
    close(fd);
}
/**
 * @brief send_SyncInfo  发送SyncInfo信息，线程安全
 * @param socketfd
 * @param id    0是创建文件夹，1是create文件，2是modify,3是删除，4是重命名
 * @param filename      全路经
 * @param newname       新文件名，发送重命名信息时才用
 * @param removedSize   删除正在发送的文件时，已发送的大小
 */
void send_SyncInfo(int socketfd,int id,string rootDir,string filename,
                   string newname,int removedSize)
{
    filesync::SyncInfo msg;
    if(id==1 || id==2)
    {//创建或修改文件才计算md5
        msg.set_md5(getFileMd5(filename));
    }
    string remoteName = filename.substr(rootDir.size());
    string remoteNewname;
    if(!newname.empty())
        remoteNewname = newname.substr(rootDir.size());
    msg.set_id(id);
    msg.set_filename(remoteName);
    msg.set_size(removedSize);
    if(4 == id) //重命名
        msg.set_newfilename(remoteNewname);
    string send = Codec::enCode(msg);
    syncInfoLock.lock();
    sysutil::writen(socketfd,send.c_str(),send.size());
    syncInfoLock.unlock();
    CHEN_LOG(INFO,"send syncinfo,id:%d,filename:%s",id,filename.c_str());
}
/**
 * @brief make_Init 客户端第一次上线，构建Init信息
 * @param socketfd
 * @param root
 * @param dir
 * @param msg_init
 */
void make_Init(int socketfd,string root,string dir,filesync::Init *msg_init)
{
    filesync::SyncInfo *msg_syncInfo;
    DIR *odir = NULL;
    if((odir = opendir(dir.c_str())) == NULL)
        CHEN_LOG(ERROR,"open dir %s error",dir.c_str());
    struct dirent *dent;
    while((dent = readdir(odir)) != NULL)
    {
        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
            continue;
        string subdir = string(dir) + dent->d_name;
        string remote_subdir = subdir.substr(root.size());
        msg_syncInfo = msg_init->add_info();
        if(dent->d_type == DT_DIR)
        {//文件夹
            msg_syncInfo->set_id(0);
            msg_syncInfo->set_filename(remote_subdir);

            make_Init(socketfd,root,(subdir + "/").c_str(),msg_init);
        }
        else
        {
            msg_syncInfo->set_id(1);
            msg_syncInfo->set_filename(remote_subdir);
            msg_syncInfo->set_md5(getFileMd5(subdir));
        }
    }
}

/**
 * @brief sync_Dir  同步整个文件夹(不包括文件夹本身)
 * @param socketfd  控制通道的socket
 * @param root  同步的顶层文件夹，发送消息时要去掉
 * @param dir  要同步的文件夹，路径后面要有/
 * @return
 */
void sync_Dir(int socketfd,string root,string dir)
{
    DIR *odir = NULL;
    if((odir = opendir(dir.c_str())) == NULL)
        CHEN_LOG(ERROR,"open dir %s error",dir.c_str());
    struct dirent *dent;
    while((dent = readdir(odir)) != NULL)
    {
        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
            continue;
        string subdir = string(dir) + dent->d_name;
        if(dent->d_type == DT_DIR)
        {//文件夹
            send_SyncInfo(socketfd,0,root,subdir);
            sync_Dir(socketfd,root,(subdir + "/").c_str());
        }
        else
        {
            send_SyncInfo(socketfd,1,root,subdir);
        }

    }
}

/**
 * @brief getFileMd5    利用qt库获取md5，支持大文件
 * @param filePath      文件名
 * @return
 */
string getFileMd5(string filePath)
{
    QFile localFile(QString::fromStdString(filePath));

    if (!localFile.open(QFile::ReadOnly))
    {
        CHEN_LOG(WARN,"file open error:%s",filePath.c_str());
    }

    QCryptographicHash ch(QCryptographicHash::Md5);

    quint64 totalBytes = 0;
    quint64 bytesWritten = 0;
    quint64 bytesToWrite = 0;
    quint64 loadSize = 1024 * 4;
    QByteArray buf;

    totalBytes = localFile.size();
    bytesToWrite = totalBytes;

    while (1)
    {
        if(bytesToWrite > 0)
        {
            buf = localFile.read(qMin(bytesToWrite, loadSize));
            ch.addData(buf);
            bytesWritten += buf.length();
            bytesToWrite -= buf.length();
            buf.resize(0);
        }
        else
        {
            break;
        }

        if(bytesWritten == totalBytes)
        {
            break;
        }
    }

    localFile.close();
    QString md5 = ch.result().toHex();
    return md5.toStdString();
}
//给文件名前面加上.
string getRemovedFilename(string rootDir,string remoteName)
{
    int pos = remoteName.find_last_of('/');
    string parDir = remoteName.substr(0,pos+1);
    string subfile = remoteName.substr(pos+1);
    string filename = rootDir+parDir+"."+subfile;
    while(access(filename.c_str(),F_OK) != 0)
    {
        pos = filename.find_last_of('/');
        parDir = filename.substr(0,pos+1);
        subfile = filename.substr(pos+1);
        filename = rootDir+parDir+"."+subfile;;
    }
    return filename;
}
//把文件名前面的.都去掉
string getOriginName(string filename)
{
    int pos = filename.find_last_of('/')+1;
    string parDir = filename.substr(0,pos);
    while(pos<filename.size() && filename[pos]=='.')
        pos++;
    string subfile = filename.substr(pos);
    return parDir+subfile;
}

}

