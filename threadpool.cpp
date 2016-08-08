#include "threadpool.h"
#include "mutexlockguard.h"

MutexLock ThreadPool::mutex;
Condition ThreadPool::cond(mutex);
queue<SyncTask> ThreadPool::taskQueue;

ThreadPool::ThreadPool(int threadNum):threadNum(threadNum)
{
    tid = (pthread_t*)malloc(sizeof(pthread_t) * threadNum);
    for(int i=0;i<threadNum;i++)
        pthread_create(&tid[i],NULL,threadFunc,NULL);
}


//线程回调函数
void *ThreadPool::threadFunc(void *data)
{
    pthread_t tid = pthread_self();
    SyncTask task;
    while (1)
    {
        {
            MutexLockGuard mutexLock(mutex);
            while (taskQueue.size() == 0)
            {
                cond.wait();
            }
            //取出一个任务并处理之
            task = std::move(taskQueue.front());
            taskQueue.pop();
        }
        CHEN_LOG(DEBUG,"tid %lu run", tid);
        task(); /** 执行任务 */
        CHEN_LOG(DEBUG,"tid:%lu idle", tid);
    }
    return (void*)0;
}

void ThreadPool::AddTask(SyncTask task)
{
    MutexLockGuard mutexLock(mutex);
    taskQueue.push(std::move(task));
    cond.notify();
}

int ThreadPool::getTaskSize()
{
    return taskQueue.size();
}



