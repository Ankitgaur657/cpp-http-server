#include "../include/ThreadPool.hpp"


ThreadPool::ThreadPool(int threads)
{

    stop = false;



    for(int i=0;i<threads;i++)
    {


        workers.emplace_back(

        [this]()
        {


            while(true)
            {


                function<void()> task;



                {

                    unique_lock<mutex> lock(queueMutex);



                    condition.wait(
                    lock,
                    [this]()
                    {
                        return stop || !tasks.empty();
                    });



                    if(stop && tasks.empty())
                        return;



                    task = move(tasks.front());

                    tasks.pop();


                }



                task();


            }


        }


        );

    }


}

void ThreadPool::enqueue(function<void()> task)
{


    {

        lock_guard<mutex> lock(queueMutex);


        tasks.push(task);

    }



    condition.notify_one();


}

ThreadPool::~ThreadPool()
{


    {

        lock_guard<mutex> lock(queueMutex);

        stop=true;

    }


    condition.notify_all();



    for(thread &worker: workers)
    {

        worker.join();

    }



}