#include "eventloop.h"
#include<sys/inotify.h>
#include<sys/epoll.h>
#include<iostream>
#include <sys/types.h>
#include <dirent.h>
#include<unordered_map>
#include "common.h"

using namespace std;
EventLoop::EventLoop()
{
    watch_init(MASK,"/home/chen/syncClient");
    epoll_fd = epoll_create(1);
    if(epoll_fd < 0)
        CHEN_LOG(ERROR,"epoll create error");
    struct epoll_event ev;
    memset(&ev,0,sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    int ctl = epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&ev);     //监控inotify
    if(ctl<0)
        CHEN_LOG(ERROR,"epoll ctl error");
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

void EventLoop::watch_init(int mask, char *root)
{
    fd = inotify_init();
    if(fd < 0)
        CHEN_LOG(ERROR,"inotify init error");
    addWatch(root,fd,mask);
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

void EventLoop::handle_sync(char *filename)
{
    cout<<"handle_sync............."<<filename<<endl;
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
                CHEN_LOG(DEBUG,"the file/directory %s was rename\n",event->name);
            }
            if(event->mask & IN_MODIFY)
            {
                CHEN_LOG(DEBUG,"thie file/directory %s was modified\n",event->name);
                handle_sync(event->name);
            }
            if(event->mask & IN_CREATE)
            {
                CHEN_LOG(DEBUG,"thie file/directory %s was created\n",event->name);
                if(event->mask & IN_ISDIR)
                {//这是一个文件夹
                    append_dir(fd,event,MASK);
                }
                handle_sync(event->name);
            }
            if(event->mask & IN_DELETE)
            {
                CHEN_LOG(DEBUG,"thie file/directory %s was deleted\n",event->name);
                if(event->mask & IN_ISDIR)
                {
                    CHEN_LOG(DEBUG,"this is dir");
                }
                handle_sync(event->name);
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
