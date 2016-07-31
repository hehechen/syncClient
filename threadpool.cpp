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
    while (1)
    {
        {
            MutexLockGuard mutexLock(mutex);
            while (taskQueue.size() == 0)
            {
                cond.wait();
            }
            CHEN_LOG(DEBUG,"tid %lu run", tid);
            //取出一个任务并处理之
            SyncTask task = std::move(taskQueue.front());
            task(); /** 执行任务 */
            taskQueue.pop();
            CHEN_LOG(DEBUG,"tid:%lu idle/n", tid);
        }
    }
    return (void*)0;
}

void ThreadPool::AddTask(SyncTask task)
{
    MutexLockGuard mutexLock(mutex);
    taskQueue.push(std::move(task));
}

int ThreadPool::getTaskSize()
{
    return taskQueue.size();
}



