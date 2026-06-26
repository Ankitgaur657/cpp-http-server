#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP


#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
using namespace std;

class ThreadPool
{


private:

    vector<thread> workers;


    queue<function<void()>> tasks;


    mutex queueMutex;


    condition_variable condition;


    bool stop;



public:


    ThreadPool(int threads);


    void enqueue(function<void()> task);


    ~ThreadPool();



};



#endif