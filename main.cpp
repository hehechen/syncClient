#include "eventloop.h"
#include "threadpool.h"

int main(int argc,char *argv[])
{
    ThreadPool threadPool(3);
    EventLoop loop(&threadPool);   //最后要有/
    while(1)
        loop.loop_once();
    return 0;
}
