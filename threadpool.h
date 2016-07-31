#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <functional>
#include <queue>
#include "mutexlock.h"
#include "condition.h"
using namespace std;

typedef function<void()> SyncTask;
class ThreadPool
{
public:
    ThreadPool(int threadNum);
    void AddTask(SyncTask task);
    int getTaskSize();
private:
    static queue<SyncTask> taskQueue;  //同步任务队列
    int threadNum;  //线程数
    static MutexLock mutex;    //互斥锁
    static Condition cond;     //条件变量
    static void *threadFunc(void *data);    //线程回调函数
    pthread_t *tid;             //tid
};

#endif // THREADPOOL_H
