//#include "../TimerHeap.h"
//#include <iostream>

//using namespace std;
//int main()
//{
//    auto print1 = [] {cout << "hehe111" << endl; };
//    auto print2 = [] {cout << "hehe222" << endl; };
//    auto print3 = [] {cout << "hehe333" << endl; };
//    auto print4 = [] {cout << "hehe444" << endl; };
//    TimerHeap timer;
//    TimerId id1 = timer.runAfter(2000000, print2);
//    cout<<id1.second.getMicrosecondsSinceEpoch()<<endl;
//    TimerId id2 = timer.runAfter(5, print1);
//    cout<<id2.second.getMicrosecondsSinceEpoch()<<endl;
//    timer.runAfter(2345, print3);
//    timer.runAfter(20000000, print4);
//    timer.cancle(id1);
//    while(1);
//    return 0;
//}
