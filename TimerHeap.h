#ifndef TIMER_HEAP_H
#define TIMER_HEAP_H
#include <functional>
#include <vector>
#include <atomic>
#include <thread>
#include <unordered_map>

#include "TimeStamp.h"
#include "common.h"
#include "mutexlock.h"
#include "condition.h"
#include "mutexlockguard.h"
//用最小堆和timerfd实现定时器



typedef std::function<void()> TimerCallback;
typedef std::pair<TimeStamp,TimerCallback> Entry;

struct EntryComp{
    bool operator()(const Entry &a,const Entry &b)
    {
        return a.first > b.first;
    }
};

struct VecComp{
    bool operator()(const Entry &a,const Entry &b)
    {
        return a.first < b.first;
    }
};

typedef std::pair<int64_t,TimeStamp> TimerId;
class TimerHeap
{
public:
    TimerHeap();    //开启监听线程
    ~TimerHeap();
    int getTimerFd()	{ return timerFD; }
    TimerId addTimer(TimeStamp when,TimerCallback cb);
    TimerId runAfter(double microSeconds,TimerCallback cb)
    {
        return addTimer(addTime(TimeStamp::now(),microSeconds),cb);
    }

    void cancle(TimerId id);

private:
    int timerFD;
    MutexLock mutex;
    Condition cond;
    //堆的底层结构,根据超时时间排序,因此只需监听第一个是否超时即可
    std::vector<Entry> timers;
    //  addTimer后返回TimerId，用来区分定时任务，方便cancle
    int64_t timerseq_;
    void *loop_timer();
    //当时间到了，timerfd变为可读时执行此函数，在这个函数里执行用户注册的回调函数
    //并修改set的内容
    void handle_read();
    void reset();       //更新定时器set
};

#endif // TIMER_HEAP_H
