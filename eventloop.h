#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include<iostream>
#include <sys/epoll.h>
#include<unordered_map>
#include<sys/inotify.h>
#include <functional>
#include <tuple>
#include <muduo/net/Buffer.h>
#include <muduo/base/Timestamp.h>

#include "socket.h"
#include "protobuf/filesync.pb.h"
#include "codec.h"
#include "threadpool.h"
#include "TimeStamp.h"
#include "TimerHeap.h"

using namespace std;

//文件传输socket的结构体
struct SocketState
{
    bool isIdle = true;    //是否可发送
    bool isRecving = false; //是否正在接收文件
    muduo::net::Buffer *inputBuffer = NULL;    //应用层的输入缓冲区
    int totalSize = 0;
    int remainSize = 0;
    string filename;        //本地文件路径包括文件名
};

typedef shared_ptr<SocketState> SocketStatePtr ;
//socket接收到信息的回调函数
typedef function<void(int,muduo::net::Buffer*)> MessageCallback;
//相应protobuf消息的回调函数
typedef shared_ptr<filesync::SyncInfo> SyncInfoPtr;
typedef shared_ptr<filesync::SendFile> sendfilePtr;
typedef shared_ptr<filesync::FileInfo> FileInfoPtr;

//用来监听inotify事件和【接收服务端同步命令的socket】
class EventLoop
{
public:
    EventLoop(char *root,ThreadPool *threadPool);
    ~EventLoop();
    void loop_once();
private:
    string rootDir;    //要同步的文件夹
    ThreadPool *threadPool;
    MutexLock mutex;    //互斥锁
    Condition cond;     //条件变量
    muduo::net::Buffer inputBuffer[3];
    Codec codec;
    TimerHeap timerHeap;

    int epoll_fd;
    struct epoll_event evs[4];
    int fd;         //inotify的fd
    TcpSocket tcpSocket[3];
    int ControlSocket = -1; //控制通道socket
    int sockfd[3];  //三个socket

    //初始化监听socket，并指定相关的回调函数
    void initSocketEpoll();
    //客户端刚与服务端连接时,发送Init消息给服务端
    void init();
    //有消息到来时的执行函数
    void onMessage(int socketfd, muduo::net::Buffer *inputBuffer);
    //接收到服务端同步命令的回调函数
    void recvcmdCallback(int socketfd,MessagePtr message);

    void recvFile(SocketStatePtr &ssptr,muduo::net::Buffer *inputBuffer);
    //获取空闲的文件传输socket
    int getIdleSocket();

    //接收到相应protobuf的处理函数
    void onIsControl(int socketfd,MessagePtr message);
    void onSyncInfo(int socketfd,MessagePtr message);
    void onSendFile(int socketfd,sendfilePtr message);
    void onFileInfo(int socketfd,FileInfoPtr message);

    unordered_map<int,string> cookieMap;    //cookie和原文件名的map
    unordered_map<int,string> dirmap;  //wd和目录的map
    //存储文件socket的状态(是否可发送)true为空闲
    unordered_map<int,SocketStatePtr>  socket_stateMap;
    //存储0.2s内有操作的文件,在完成执行函数后就erase掉
    unordered_map<string,TimerId> fileopMap;

    static const int MASK = IN_MODIFY | IN_CREATE | IN_MOVED_FROM | IN_MOVED_TO |
            IN_DELETE | IN_DELETE_SELF;    //要监听的事件
    static const int EVENT_SIZE = sizeof (struct inotify_event);
    static const int BUF_LEN = 1024 * ( EVENT_SIZE + 16 );

    void watch_init(int mask,char *root);   //初始化监听目录
    void addWatch(const char *dir, int fd, int mask);       //递归添加监听目录
    char buffer[BUF_LEN];
    void doAction();            //根据inotify的事件采取行动
    void handle_create(string filename, bool isDir);
    void handle_modify(string filename);
    void handle_rename(string oldname, string newname);
    void handle_delete(string filename, bool isDir);
};



#endif // EVENTLOOP_H
