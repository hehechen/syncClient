#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include<iostream>
#include <sys/epoll.h>
#include<unordered_map>
#include<sys/inotify.h>

using namespace std;
//用来监听inotify事件和【接收服务端同步命令的socket】
class EventLoop
{
public:
    EventLoop();
    ~EventLoop();
    void loop_once();
private:
    int fd;         //inotify的fd
    int socket_fd;  //接收服务端同步命令的socket
    unordered_map<int,string> dirmap;  //wd和目录的map
    static const int MASK = IN_MODIFY | IN_CREATE | IN_MOVED_FROM | IN_DELETE
             | IN_DELETE_SELF;    //要监听的事件
    static const int EVENT_SIZE = sizeof (struct inotify_event);
    static const int BUF_LEN = 1024 * ( EVENT_SIZE + 16 );
    void watch_init(int mask,char *root);   //初始化监听目录
    void addWatch(char *dir, int fd, int mask);       //递归添加监听目录
    void append_dir(int fd, struct inotify_event *event, int mask);//添加监听文件夹
    void handle_sync(char *filename);             //文件目录发生改变则执行同步事件
    int epoll_fd;
    struct epoll_event evs[2];
    char buffer[BUF_LEN];
    void doAction();            //根据inotify的事件采取行动
};

#endif // EVENTLOOP_H
