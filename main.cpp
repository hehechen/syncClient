#include "eventloop.h"
#include "threadpool.h"

int main(int argc,char *argv[])
{
    ThreadPool threadPool(3);
    if(argc>1)
    {
        string ip(argv[1]);
        EventLoop loop("/home/chen/Desktop/client/",&threadPool,ip);   //最后要有/
        while(1)
        {
            loop.loop_once();
        }
    }
    else
    {
        EventLoop loop("/home/chen/Desktop/client/",&threadPool);   //最后要有/
        while(1)
        {
            loop.loop_once();
        }
    }
}
