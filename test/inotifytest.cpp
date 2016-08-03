#include "eventloop.h"
#include "threadpool.h"

int main()
{
    ThreadPool threadPool(3);
    EventLoop loop("/home/chen/Desktop/client/",&threadPool);   //最后要有/
    while(1)
    {
        loop.loop_once();
    }
}
