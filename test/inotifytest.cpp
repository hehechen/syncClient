#include "eventloop.h"

int main()
{
    EventLoop loop("/home/chen/build-syncClient-Desktop_Qt_5_6_1_GCC_64bit-Debug/");
    while(1)
    {
        loop.loop_once();
    }
}
