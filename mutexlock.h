#ifndef MUTEXLOCK_H
#define MUTEXLOCK_H
#include <pthread.h>
/**
 * @author chen
 * 用RAII封装互斥锁
 */
class MutexLock
{
public:
    MutexLock()     {   pthread_mutex_init(&mutex,nullptr); }
    ~MutexLock()    {   pthread_mutex_destroy(&mutex);  }
    pthread_mutex_t *getMutex() {   return &mutex;  }
    void lock()
    {//仅供MutexGuard调用
        pthread_mutex_lock(&mutex);
    }
    void unlock()
    {//仅供MutexGuard调用
        pthread_mutex_unlock(&mutex);
    }

private:
    pthread_mutex_t mutex;
};


#endif // MUTEXLOCK_H
