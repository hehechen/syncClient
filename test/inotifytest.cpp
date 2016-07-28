#include "eventloop.h"

int main()
{
    EventLoop loop;
    while(1)
    {
        loop.loop_once();
    }
}
