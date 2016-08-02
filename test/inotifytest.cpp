#include "eventloop.h"
#include "threadpool.h"

int main()
{
    ThreadPool threadPool(3);
    EventLoop loop("/home/chen/build-syncClient-Desktop_Qt_5_6_1_GCC_64bit-Debug/",&threadPool);
    while(1)
    {
        loop.loop_once();
    }
}
