//#include<iostream>
//#include"threadpool.h"

//using namespace std;

//int main()
//{
//    ThreadPool pool(3);
//    auto print1 = []{cout<<"hehe1"<<endl;};
//    auto print2 = []{cout<<"hehe2"<<endl;};
//    auto print3 = []{cout<<"hehe3"<<endl;};
//    auto print4 = []{cout<<"hehe4"<<endl;};
//    auto print5 = []{cout<<"hehe5"<<endl;};
//    pool.AddTask(print1);
//    pool.AddTask(print2);
//    pool.AddTask(print3);
//    pool.AddTask(print4);
//    pool.AddTask(print5);

//    while(1)
//    {
//        if(pool.getTaskSize() != 0)
//            printf("there are %d task running\n",pool.getTaskSize());
//    }
//    return 0;
//}
