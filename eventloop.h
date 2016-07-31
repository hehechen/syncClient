#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include<iostream>
#include <sys/epoll.h>
#include<unordered_map>
#include<sys/inotify.h>
#include <functional>
#include <muduo/net/Buffer.h>
using namespace std;


//socket接收到信息的回调函数
typedef function<void(int,muduo::net::Buffer*)> MessageCallback;

//用来监听inotify事件和【接收服务端同步命令的socket】
class EventLoop
{
public:
    EventLoop(char *root);
    ~EventLoop();
    void loop_once();
private:
    int epoll_fd;
    struct epoll_event evs[2];
    int fd;         //inotify的fd
    int sockfd[3];  //三个socket
    muduo::net::Buffer inputBuffer[3];  //三个socket的缓冲区

    //控制通道和文件传输通道的回调函数
    static MessageCallback onControlMessage;
    static MessageCallback onFileMessage;
    //初始化监听socket，并指定相关的回调函数
    void initSocketEpoll();
    int socket_fd;  //接收服务端同步命令的socket

    unordered_map<int,string> cookieMap;    //cookie和原文件名的map
    unordered_map<int,string> dirmap;  //wd和目录的map

    static const int MASK = IN_MODIFY | IN_CREATE | IN_MOVED_FROM | IN_MOVED_TO |
            IN_DELETE | IN_DELETE_SELF;    //要监听的事件
    static const int EVENT_SIZE = sizeof (struct inotify_event);
    static const int BUF_LEN = 1024 * ( EVENT_SIZE + 16 );

    void watch_init(int mask,char *root);   //初始化监听目录
    void addWatch(char *dir, int fd, int mask);       //递归添加监听目录
    void append_dir(int fd, struct inotify_event *event, int mask);//添加监听文件夹
    char buffer[BUF_LEN];
    void doAction();            //根据inotify的事件采取行动
    void handle_create(char *filename,bool isDir);
    void handle_modify(char *filename);
    void handle_rename(const char *oldname,const char *newname);
    void handle_delete(char *filename,bool isDir);
};



#endif // EVENTLOOP_H
