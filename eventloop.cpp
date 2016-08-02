#include<sys/inotify.h>
#include<sys/epoll.h>
#include<iostream>
#include <sys/types.h>
#include <dirent.h>
#include<unordered_map>

#include "eventloop.h"
#include "common.h"
#include "socket.h"
#include "sysutil.h"
#include "mutexlockguard.h"

using namespace std;

EventLoop::EventLoop(char *root, ThreadPool *threadPool):
    threadPool(threadPool),cond(mutex)
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
        else
        {
            muduo::net::Buffer *inputBuffer = socket_stateMap[evs[i].data.fd]->inputBuffer;
            if(inputBuffer->readFd(evs[i].data.fd,NULL) < 0)
                CHEN_LOG(ERROR,"read to buffer error");
            onMessage(evs[i].data.fd,inputBuffer);
        }
    }
}
//有数据到来，判断是消息还是文件数据
void EventLoop::onMessage(int socketfd,muduo::net::Buffer *inputBuffer)
{
    SocketStatePtr ssptr = socket_stateMap[socketfd];
    if(ssptr->isRecving)//说明是文件数据
        recvFile(ssptr,inputBuffer);
    else            //不管是控制通道还是数据传输通道都执行此函数
        codec.parse(socketfd,inputBuffer);
}

/**
 * @brief EventLoop::initSocketEpoll    初始化监听socket并注册解码器的回调函数
 */
void EventLoop::initSocketEpoll()
{
    //连接三个socket并监听
    for(int i=0;i<3;i++)
    {
        sockfd[i] = tcpSocket[i].connect(string("127.0.0.1"),8888);
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
    //初始化文件传输socket的状态
    for(int i=0;i<3;i++)
    {
        SocketStatePtr initptr(new SocketState);
        initptr->inputBuffer = &inputBuffer[i];
        socket_stateMap.insert(make_pair(sockfd[i],initptr));
    }
    codec.registerCallback<filesync::isControl>(std::bind(&EventLoop::onIsControl,this,
                               std::placeholders::_1,std::placeholders::_2));
    codec.registerCallback<filesync::sendfile>(std::bind(&EventLoop::onSendFile,this,
                               std::placeholders::_1,std::placeholders::_2));
    codec.registerCallback<filesync::fileInfo>(std::bind(&EventLoop::onFileInfo,this,
                               std::placeholders::_1,std::placeholders::_2));

}
/**
 * @brief EventLoop::recvFile   从Buffer中接收文件数据
 * @param ssptr
 * @param inputBuffer
 */
void EventLoop::recvFile(SocketStatePtr &ssptr, muduo::net::Buffer *inputBuffer)
{
    int len = inputBuffer->readableBytes();
    if(len >= ssptr->remainSize)
    {//文件接受完
        sysutil::fileRecvfromBuf(ssptr->filename.c_str(),
                                 inputBuffer->peek(),ssptr->remainSize);
        ssptr->isRecving = false;
        ssptr->remainSize = 0;
        ssptr->totalSize = 0;
        ssptr->filename.clear();
    }
    else
    {
        sysutil::fileRecvfromBuf(ssptr->filename.c_str(),
                                 inputBuffer->peek(),len);
        ssptr->remainSize -= len;
    }
}
/**
 * @brief EventLoop::getIdleSocket  获取空闲的文件传输socket
 * @return  找不到就返回-1
 */
int EventLoop::getIdleSocket()
{
    for(int i=1;i<3;i++)
    {
        if(socket_stateMap[sockfd[i]]->isIdle)
            return sockfd[i];
    }
    return -1;
}

void EventLoop::onIsControl(int socketfd, MessagePtr message)
{
    CHEN_LOG(DEBUG,"controlfd %d",socketfd);
    ControlSocket = socketfd;
}
//接收到sendfile消息,选择一个空闲的socket传输文件
void EventLoop::onSendFile(int socketfd, sendfilePtr message)
{
    CHEN_LOG(DEBUG,"receive sendfile=========");
    auto func_send = [this,message](){
        int send_sockfd;
        SocketStatePtr ssptr;
        {//睡眠直到获取空闲socket
            MutexLockGuard mutexLock(mutex);
            while((send_sockfd = getIdleSocket()) == -1)
                cond.wait();
            ssptr = socket_stateMap[send_sockfd];
            ssptr->isIdle = false;
        }
        string filename = message->filename();
        sysutil::sendfileWithproto(send_sockfd,filename.c_str());
        //将socket设为空闲并唤醒其它在等待的线程
        ssptr->isIdle = true;
        cond.notify();
    };
    threadPool->AddTask(std::move(func_send));
}
//接收到fileInfo消息，设置socket_stateMap，注意此时消息已从Buffer中清除
void EventLoop::onFileInfo(int socketfd, fileInfoPtr message)
{
    SocketStatePtr ssptr = socket_stateMap[socketfd];
    ssptr->isRecving = true;
    ssptr->filename = std::move(message->filename());
    ssptr->totalSize = ssptr->remainSize = message->size();
    recvFile(ssptr,ssptr->inputBuffer);
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
    if(wd<0)
        CHEN_LOG(ERROR,"inotify add error");
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
                string filename = std::move(string(event->name));
                //先判断过去0.2s是否有对相同文件的修改
                auto it = fileopMap.find(filename);
                //在0.2s后执行
                TimerId id = timerHeap.runAfter(200000,
                            std::bind(&EventLoop::handle_modify,this,filename));
                //更新fileopMap
                if(it != fileopMap.end())
                {//删除之前的定时任务
                    timerHeap.cancle(it->second);
                    it->second = id;
                }
                else
                {
                    fileopMap.insert(make_pair(filename,id));
                }
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
    {//发送syncInfo信息给服务端
        filesync::syncInfo msg;
        msg.set_id(1);
        msg.set_filename(string(filename));
        string send = codec.enCode(msg);
        sysutil::writen(ControlSocket,send.c_str(),send.size());
        CHEN_LOG(DEBUG,"thie file %s was created\n",filename);
    }
}
/**
 * @brief EventLoop::handle_modify  修改文件内容的同步任务，更新fileopMap
 * @param filename
 */
void EventLoop::handle_modify(string filename)
{
    CHEN_LOG(DEBUG,"thie file %s was modified\n",filename.c_str());
    //发送syncInfo信息给服务端
    filesync::syncInfo msg;
    msg.set_id(2);
    msg.set_filename(filename);
    string send = codec.enCode(msg);
    sysutil::writen(ControlSocket,send.c_str(),send.size());
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
