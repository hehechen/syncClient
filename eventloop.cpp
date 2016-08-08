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

EventLoop::EventLoop(ThreadPool *threadPool):
    threadPool(threadPool),cond(mutex),pc(ParseConfig::getInstance())
{
    pc->loadfile();
    rootDir = pc->getSyncRoot();
    ip = pc->getServerIp();
    epoll_fd = epoll_create(1);
    if(epoll_fd < 0)
        CHEN_LOG(ERROR,"epoll create error");
    watch_init(MASK,rootDir.c_str());
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
/**
 * @brief EventLoop::isRemoved 查询tid_fileMaps,查看tid线程正在发送的文件是否已被删除
 * @param tid
 * @param filename
 * @return
 */
bool EventLoop::isRemoved(pthread_t tid, string filename)
{
    bool ret = true;
    get_tidMapsLock();
    if(tid_fileMaps[tid] == filename)
        ret = false;
    free_tidMapsLock();
    return ret;
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
        sockfd[i] = tcpSocket[i].connect(ip,8888);
        if(sockfd[i] < 0)
            CHEN_LOG(ERROR,"connect error");
        tcpSocket[i].setKeepalive();
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
    codec.registerCallback<filesync::IsControl>(std::bind(&EventLoop::onIsControl,this,
                                                          std::placeholders::_1,std::placeholders::_2));
    codec.registerCallback<filesync::SendFile>(std::bind(&EventLoop::onSendFile,this,
                                                         std::placeholders::_1,std::placeholders::_2));
    codec.registerCallback<filesync::FileInfo>(std::bind(&EventLoop::onFileInfo,this,
                                                         std::placeholders::_1,std::placeholders::_2));
    codec.registerCallback<filesync::SyncInfo>(std::bind(&EventLoop::onSyncInfo,this,
                                                         std::placeholders::_1,std::placeholders::_2));

}

void EventLoop::init()
{
    filesync::Init msg;
    msg.set_username(pc->getUsername());
    msg.set_password(pc->getPassword());
    sysutil::make_Init(ControlSocket,rootDir,rootDir,&msg);
    string send = Codec::enCode(msg);
    sysutil::writen(ControlSocket,send.c_str(),send.size());
    CHEN_LOG(DEBUG,"SEND INIT");
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
        inputBuffer->retrieve(ssptr->remainSize);
        ssptr->isRecving = false;
        ssptr->remainSize = 0;
        ssptr->totalSize = 0;
        ssptr->filename.clear();
    }
    else
    {
        sysutil::fileRecvfromBuf(ssptr->filename.c_str(),
                                 inputBuffer->peek(),len);
        inputBuffer->retrieve(len);
        ssptr->remainSize -= len;
    }
}
/**
 * @brief EventLoop::getIdleSocket  获取空闲的文件传输socket
 * @return  找不到就返回-1
 */
int EventLoop::getIdleSocket()
{
    for(int i=0;i<3;i++)
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
    socket_stateMap[socketfd]->isIdle = false;  //将控制通道socket设置为不可发送文件
    init();
}
/**
 * @brief EventLoop::onSyncInfo 收到服务端的同步命令，服务端若收到其它客户端的新文件或修改，就直接
 *                  从文件传输通道传给客户端。此函数只需处理删除、重命名、创建文件夹
 * @param socketfd
 * @param message
 */
void EventLoop::onSyncInfo(int socketfd, SyncInfoPtr message)
{
    int id = message->id();
    string localname = rootDir+message->filename();
    switch(id)
    {
    case 0:
    {//创建文件夹
        if(::access(localname.c_str(),F_OK) != 0)
        {
            ignore_File = localname;
            mkdir(localname.c_str(),0666);
        }
        break;
    }
    case 3:
    {//删除文件
        if(access(localname.c_str(),F_OK) != 0) //文件不存在
            CHEN_LOG(INFO,"file %s don't exit",localname.c_str());
        else
        {
            ignore_File = localname;
            char rmCmd[512];
            sprintf(rmCmd,"rm -rf %s",localname.c_str());
            system(rmCmd);
        }
        break;
    }
    case 4:
    {//重命名
        ignore_File = localname;
        string newFilename = message->newfilename();
        if(rename(localname.c_str(),(rootDir+newFilename).c_str()) < 0)
            CHEN_LOG(ERROR,"rename %s error",localname.c_str());
        CHEN_LOG(DEBUG,"rename %s to %s",localname.c_str(),
                 (rootDir+newFilename).c_str());
    }
    }
}
//接收到sendfile消息,选择一个空闲的socket传输文件
void EventLoop::onSendFile(int socketfd, SendFilePtr message)
{
    CHEN_LOG(DEBUG,"receive sendfile to send %s",message->filename().c_str());
    auto func_send = [this,message](){
        int send_sockfd;
        SocketStatePtr ssptr;
        {//睡眠直到获取空闲socket
            MutexLockGuard mutexLock(mutex);
            while((send_sockfd = getIdleSocket()) == -1)
                cond.wait();
            ssptr = socket_stateMap[send_sockfd];
        }
        ssptr->isIdle = false;
        ssptr->tid = pthread_self();
        string localname = rootDir+message->filename();
        string remotename = message->filename();
        tid_fileMaps[pthread_self()] = localname;
        sysutil::sendfileWithproto(send_sockfd,localname.c_str(),remotename.c_str(),this);
        //将socket设为空闲并唤醒其它在等待的线程
        ssptr->isIdle = true;
        ssptr->tid = -1;
        cond.notify();
    };
    threadPool->AddTask(std::move(func_send));
}
//接收到fileInfo消息，设置socket_stateMap，注意此时消息已从Buffer中清除
void EventLoop::onFileInfo(int socketfd, FileInfoPtr message)
{
    SocketStatePtr ssptr = socket_stateMap[socketfd];
    ssptr->isRecving = true;
    ssptr->filename = std::move(rootDir+message->filename());
    ssptr->totalSize = ssptr->remainSize = message->size();
    CHEN_LOG(DEBUG,"ready to receive file %s",ssptr->filename.c_str());
    recvFile(ssptr,ssptr->inputBuffer);
}

void EventLoop::watch_init(int mask,const char *root)
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

void EventLoop::addWatch(const char *dir, int fd, int mask)
{
    DIR *odir = NULL;
    if((odir = opendir(dir)) == NULL)
        CHEN_LOG(ERROR,"open dir %s error",dir);
    int wd = inotify_add_watch(fd,dir,mask);
    if(wd < 0)
        CHEN_LOG(ERROR,"inotify add %s error",dir);
    dirmap.insert(make_pair(wd,string(dir)));
    struct dirent *dent;
    while((dent = readdir(odir)) != NULL)
    {
        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
            continue;
        if(dent->d_type == DT_DIR)
        {
            char subdir[512];
            sprintf(subdir,"%s%s/",dir,dent->d_name);
            addWatch(subdir,fd,mask);
        }
    }
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
                string parDir = dirmap[event->wd];
                auto it = cookieMap.find(event->cookie);
                if(it!=cookieMap.end())
                {
                    handle_rename(parDir + it->second.c_str(),
                                  parDir + event->name);
                    cookieMap.erase(it);    //清除记录
                }
                else
                    CHEN_LOG(ERROR,"can't find rename cookie");
            }
            if(event->mask & IN_MODIFY)
            {
                string parDir = dirmap[event->wd];
                string filename = std::move(parDir + string(event->name));
                //先判断过去1.2s是否有对相同文件的修改
                auto it = fileopMap.find(filename);
                //在1.2s后执行
                TimerId id = timerHeap.runAfter(1200000,
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
                string parDir = dirmap[event->wd];
                string filename = parDir + string(event->name);
                if(event->mask & IN_ISDIR)
                {//这是一个文件夹
                    addWatch((filename+"/").c_str(),fd,MASK);
                    handle_create((filename+"/"),true);
                }
                else
                { //在1.2s后执行
                    TimerId id = timerHeap.runAfter(1200000,
                                   std::bind(&EventLoop::handle_create,this,filename,false));
                    fileopMap[filename] = id;
                }
            }
            if(event->mask & IN_DELETE)
            {
                string parDir = dirmap[event->wd];
                string filename = parDir + string(event->name);
                if(event->mask & IN_ISDIR)
                {
                    handle_delete(filename,true);
                }
                else
                    handle_delete(filename,false);
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
                CHEN_LOG(DEBUG,"rm inotify dir");
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
void EventLoop::handle_create(string filename,bool isDir)
{//发送syncInfo信息给服务端
    if(isDir)
    {
        if(filename != ignore_File)
        {
            sysutil::send_SyncInfo(ControlSocket,0,rootDir,filename);    //发送创建文件夹的信息
            sysutil::sync_Dir(ControlSocket,rootDir.c_str(),filename.c_str());
            CHEN_LOG(DEBUG,"thie directory %s was created\n",filename.c_str());
        }
        else
            ignore_File.clear();
    }
    else
    {
        //先查看此文件是不是正在下载，防止重复下载
        for(auto it=socket_stateMap.begin();it!=socket_stateMap.end();++it)
        {
            if(filename == it->second->filename && it->second->isRecving)
                return;
        }
        sysutil::send_SyncInfo(ControlSocket,1,rootDir,filename);
        CHEN_LOG(DEBUG,"thie file %s was created\n",filename.c_str());
    }
}
/**
 * @brief EventLoop::handle_modify  修改文件内容的同步任务，更新fileopMap
 * @param filename
 */
void EventLoop::handle_modify(string filename)
{
    //先查看此文件是不是正在下载，防止重复下载
    for(auto it=socket_stateMap.begin();it!=socket_stateMap.end();++it)
    {
        if(filename == it->second->filename && it->second->isRecving)
            return;
    }
    CHEN_LOG(DEBUG,"thie file %s was modified\n",filename.c_str());
    //发送SyncInfo信息给服务端
    sysutil::send_SyncInfo(ControlSocket,2,rootDir,filename);
}

void EventLoop::handle_rename(string oldname,string newname)
{
    if(oldname != ignore_File)
    {
        //发送SyncInfo重命名信息给服务端
        sysutil::send_SyncInfo(ControlSocket,4,rootDir,oldname,newname);
        CHEN_LOG(DEBUG,"thie file %s was rename to %s\n",oldname.c_str(),newname.c_str());
    }
    else
        ignore_File.clear();
}

void EventLoop::handle_delete(string filename,bool isDir)
{
    if(filename != ignore_File)
    {
        for(auto it=socket_stateMap.begin();it!=socket_stateMap.end();++it)
        {
            if(!it->second->isIdle && it->second->tid!=-1)
            {
                if(tid_fileMaps[it->second->tid] != "")
                {
                    get_tidMapsLock();
                    tid_fileMaps[it->second->tid] = "";
                    free_tidMapsLock();
                }
            }
        }
        //发送SyncInfo信息给服务端
        sysutil::send_SyncInfo(ControlSocket,3,rootDir,filename);
        if(isDir)
            CHEN_LOG(DEBUG,"thie directory %s was deleted\n",filename.c_str());
        else
            CHEN_LOG(DEBUG,"thie file %s was deleted\n",filename.c_str());
    }
    else
        ignore_File.clear();
}
