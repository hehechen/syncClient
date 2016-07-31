#include "eventloop.h"
#include<sys/inotify.h>
#include<sys/epoll.h>
#include<iostream>
#include <sys/types.h>
#include <dirent.h>
#include<unordered_map>
#include "common.h"
#include "socket.h"
#include "protobuf/filesync.init.pb.h"

using namespace std;

//控制信息到来时的回调函数
MessageCallback EventLoop::onControlMessage = [] (int socketfd,muduo::net::Buffer *inputBuffer){
cout<<endl;
};

MessageCallback EventLoop::onFileMessage = [] (int socketfd,muduo::net::Buffer*){
    cout<<endl;
};

EventLoop::EventLoop(char *root)
{
    epoll_fd = epoll_create(1);
    if(epoll_fd < 0)
        CHEN_LOG(ERROR,"epoll create error");
    watch_init(MASK,root);
    initSocketEpoll();
}

EventLoop::~EventLoop()
{
    close(fd);
}

void EventLoop::loop_once()
{
    int num = epoll_wait(epoll_fd,evs,5,10);
    for(int i=0;i<num;i++)
    {
        if(evs[i].data.fd == fd)
        {//是inotify事件
            if(evs[i].events == EPOLLIN)
            {
                doAction();
            }
        }
    }
}
/**
 * @brief EventLoop::initSocketEpoll    初始化监听socket，并指定相关的回调函数
 */
void EventLoop::initSocketEpoll()
{
    //创建三个socket并监听
    TcpSocket socket;
    for(int i=0;i<3;i++)
    {
        sockfd[i] = socket.connect(string("127.0.0.1"),8888);
        if(sockfd[i] < 0)
            CHEN_LOG(ERROR,"connect error");
        struct epoll_event ev;
        memset(&ev,0,sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd =  sockfd[i];
        int ctl = epoll_ctl(epoll_fd,EPOLL_CTL_ADD, sockfd[i],&ev);     //监控inotify
        if(ctl<0)
            CHEN_LOG(ERROR,"epoll ctl error");
    }

}

void EventLoop::watch_init(int mask, char *root)
{
    fd = inotify_init();
    if(fd < 0)
        CHEN_LOG(ERROR,"inotify init error");
    addWatch(root,fd,mask);
    struct epoll_event ev;
    memset(&ev,0,sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    int ctl = epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&ev);     //监控inotify
    if(ctl<0)
        CHEN_LOG(ERROR,"epoll ctl error");
}

void EventLoop::addWatch(char *dir, int fd, int mask)
{
    DIR *odir = NULL;
    if((odir = opendir(dir)) == NULL)
        CHEN_LOG(ERROR,"open dir %s error",dir);
    int wd = inotify_add_watch(fd,dir,mask);
    dirmap.insert(make_pair(wd,string(dir)));
    struct dirent *dent;
    while((dent = readdir(odir)) != NULL)
    {
        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
            continue;
        if(dent->d_type == DT_DIR)
        {
            char subdir[512];
            sprintf(subdir,"%s/%s",dir,dent->d_name);
            addWatch(subdir,fd,mask);
        }
    }
}
/**
 * @brief EventLoop::append_dir //添加监听文件夹
 * @param fd                //inotify的fd
 * @param event
 * @param mask
 */
void EventLoop::append_dir(int fd,struct inotify_event *event,int mask)
{
    char ndir[512];
    int wd;
    sprintf(ndir,"%s/%s",dirmap.find(event->wd)->second.c_str(),event->name);
    wd = inotify_add_watch(fd,ndir,mask);
    dirmap.insert(make_pair(wd,string(ndir)));
}

void EventLoop::doAction()
{
    int i = 0;
    int len = read(fd,buffer,BUF_LEN);
    while(i < len)
    {
        struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
        if(event->len)
        {
            if(event->mask & IN_MOVED_FROM)//重命名
            {
                cookieMap.insert(make_pair(event->cookie,string(event->name)));
            }
            if(event->mask & IN_MOVED_TO)//重命名
            {
                auto it = cookieMap.find(event->cookie);
                if(it!=cookieMap.end())
                {
                    handle_rename(it->second.c_str(),event->name);
                }
                else
                    CHEN_LOG(ERROR,"can't find rename cookie");
            }
            if(event->mask & IN_MODIFY)
            {
                handle_modify(event->name);
            }
            if(event->mask & IN_CREATE)
            {
                if(event->mask & IN_ISDIR)
                {//这是一个文件夹
                    append_dir(fd,event,MASK);
                    handle_create(event->name,true);
                }
                else
                   handle_create(event->name,false);
            }
            if(event->mask & IN_DELETE)
            {
                if(event->mask & IN_ISDIR)
                {
                    handle_delete(event->name,true);
                }
                else
                    handle_delete(event->name,false);
            }
        }
        else if(event->mask & IN_DELETE_SELF)
        {//子目录被删除,IN_DELETE_SELF的name为空
            if(inotify_rm_watch(fd,event->wd) < 0)
                CHEN_LOG(ERROR,"rm inotify error");
            auto it = dirmap.find(event->wd);
            if(it!=dirmap.end())
            {
                dirmap.erase(it);
            }
        }
        i += EVENT_SIZE + event->len;
    }
    memset(buffer,0,sizeof(buffer));
}
/**
 * @brief handle_create 创建文件的同步任务
 * @param filename
 * @param isDir 是否是文件夹
 */
void EventLoop::handle_create(char *filename,bool isDir)
{
    if(isDir)
        CHEN_LOG(DEBUG,"thie directory %s was created\n",filename);
    else
        CHEN_LOG(DEBUG,"thie file %s was created\n",filename);
}

void EventLoop::handle_modify(char *filename)
{
   CHEN_LOG(DEBUG,"thie file %s was modified\n",filename);
}

void EventLoop::handle_rename(const char *oldname,const char *newname)
{
    CHEN_LOG(DEBUG,"thie file %s was rename to %s\n",oldname,newname);
}

void EventLoop::handle_delete(char *filename,bool isDir)
{
    if(isDir)
        CHEN_LOG(DEBUG,"thie directory %s was deleted\n",filename);
    else
        CHEN_LOG(DEBUG,"thie file %s was deleted\n",filename);
}
