

//#include <sys/inotify.h>
//#include <sys/epoll.h>
//#include <iostream>
//#include <stdio.h>
//#include <unistd.h>
//#include <unordered_map>
//#include"common.h"
//using namespace std;
//#define EVENT_SIZE  ( sizeof (struct inotify_event) )
//#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

//int main()
//{
//    int i = 0;
//    int fd = inotify_init();
//    int wd = inotify_add_watch(fd,"/home/chen/syncClient",IN_ATTRIB | IN_DELETE | IN_MOVED_TO);
//    wd = inotify_add_watch(fd,"/home/chen/syncClient/hehe",IN_DELETE_SELF| IN_DELETE);
//    char buffer[BUF_LEN];
//    int epoll_fd = epoll_create(1);
//    epoll_event event;
//    event.events = EPOLLIN;
//    int ctl = epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event);
//    if(ctl<0)
//        CHEN_LOG(ERROR,"epoll error");
//    int num_events = epoll_wait(epoll_fd,&event,5,100000);
//    if(num_events>0)
//    {
//        if(event.events == EPOLLIN)
//        {
//            size_t len = read(fd,buffer,BUF_LEN);
//            while(i < len)
//            {
//                struct inotify_event *inotify_ev = ( struct inotify_event * ) &buffer[ i ];
//                if(inotify_ev->len)
//                {
//                    if(inotify_ev->mask & IN_MOVED_TO)
//                        printf("the file/directory %s was rename\n",inotify_ev->name);

//                }
//                else if(inotify_ev->mask & IN_DELETE_SELF)
//                {
//                    printf("thie file/directory %s was DELETED\n",inotify_ev->name);
//                }
//                i += EVENT_SIZE + inotify_ev->len;
//            }
//        }
//    }

//    inotify_rm_watch(fd,wd);
//    close(fd);
//    return 0;
//}
