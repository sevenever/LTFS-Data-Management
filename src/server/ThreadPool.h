/*******************************************************************************
 * Copyright 2018 IBM Corp. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************/
#pragma once

/** @page thread_pool ThreadPool

    The ThreadPool class is designated as a facility to start threads
    and wait for their completion. It has the following capabilities
    and limitations:

    - Only a single function can be specified to be executed by the
      threads, parameters can be different.
    - A single name can be specified for the threads.
    - A thread can be reused up to 10 seconds of inactivity. Thereafter
      it will terminate.

    For the constructor of the class the following parameters need to
    be specified:

    - the function to be executed
    - the maximum number of threads
    - the name of the threads

    A new thread can be enqueued with the ThreadPool::enqueue method.
    Only the function (that has been specified with the constructor)
    parameters and if necessary (if not Const::UNSET should be specified)
    a request number as a first parameter. For ThreadPool::waitCompletion
    method a request number can be specified to wait only for for specific
    request to finish (if not Const::UNSET should be specified). This only is
    the case for those ThreadPools that perform tasks for different request at
    the same time.

    The ThreadPool is used by doing the following three steps:

    - Setup the ThreadPool within its @ref ThreadPool::ThreadPool "constructor"
      with a name and a function to be executed.
    - Execute this function in a thread by the ThreadPool::enqueue method
      and specifying a request number (use Const::UNSET if not required) and
      function parameters.
    - Wait for all threads to complete by using the ThreadPool::waitCompletion
      method.

 */

template<typename ... Args> class ThreadPool
{
private:
    std::mutex enqueue_mtx; // seems to be necessary

    std::mutex mtx_main;
    std::condition_variable cond_main;
    std::condition_variable cond_init;
    std::mutex mtx_resp;
    std::condition_variable cond_resp;
    std::condition_variable cond_cont;
    std::condition_variable cond_fin;

    std::packaged_task<void()> task;
    std::atomic<int> started;
    std::atomic<int> occupied;
    std::atomic<bool> new_work;
    std::atomic<int> global_req_num;
    std::map<int, long> numJobs;

    const std::function<void(Args ... args)> func;
    const int num_thrds;
    std::atomic<int> num_thrds_started;
    std::thread *new_thread;
    std::thread *last_thread;
    const std::string name;

    void threadfunc()
    {
        int req_num;
        std::thread *t = last_thread;

        pthread_setname_np(pthread_self(), name.c_str());

        std::packaged_task < void() > ltask;
        std::unique_lock < std::mutex > lock(mtx_main);
        started++;
        cond_init.notify_one();

        while (true) {
            cond_main.wait_for(lock, Const::IDLE_THREAD_LIVE_TIME);
            if (new_work == false) {
                lock.unlock();
                if (t == nullptr) {
                    num_thrds_started--;
                    cond_main.notify_all();
                    return;
                }
                num_thrds_started--;
                cond_main.notify_all();
                t->join();
                delete (t);
                return;
            }
            new_work = false;
            occupied++;
            req_num = global_req_num;
            numJobs[req_num]++;

            ltask = std::move(task);

            lock.unlock();

            std::unique_lock < std::mutex > lock2(mtx_resp);
            cond_resp.notify_one();
            lock2.unlock();

            ltask();
            ltask.reset();

            lock.lock();

            occupied--;
            numJobs[req_num]--;
            cond_fin.notify_all();
            if (occupied == num_thrds - 1)
                cond_cont.notify_one();
        }
    }

public:
    ThreadPool(std::function<void(Args ... args)> func_, int num_thrds_,
            std::string name_) :
            started(0), occupied(0), new_work(false), func(func_), num_thrds(
                    num_thrds_), num_thrds_started(0), new_thread(nullptr), last_thread(
                    nullptr), name(name_)

    {
    }

    void enqueue(int req_num, Args ... args)
    {
        std::lock_guard < std::mutex > elock(enqueue_mtx);
        std::unique_lock < std::mutex > lock(mtx_main);
        new_work = true;
        if (occupied == num_thrds)
            cond_cont.wait(lock);

        if (num_thrds_started == occupied) {
            new_thread = new std::thread(&ThreadPool::threadfunc, this);
            num_thrds_started++;
            cond_init.wait(lock);
            last_thread = new_thread;
            new_thread = nullptr;
        }

        global_req_num = req_num;

        std::packaged_task < void() > task_(std::bind(func, args ...));
        task = std::move(task_);

        std::unique_lock < std::mutex > lock2(mtx_resp);
        cond_main.notify_one();
        lock.unlock();
        cond_resp.wait(lock2);
    }

    void waitCompletion(int req_num)

    {
        std::unique_lock < std::mutex > lock(mtx_main);
        cond_fin.wait(lock, [this, req_num] {return numJobs[req_num] == 0;});
        numJobs.erase(req_num);
    }

    ~ThreadPool()
    {
        if (last_thread != nullptr) {
            try {
                std::unique_lock < std::mutex > lock(mtx_main);

                cond_main.wait(lock, [this] {return num_thrds_started == 0;});
                last_thread->join();
                delete (last_thread);
                last_thread = nullptr;
            } catch (const std::exception& e) {
                TRACE(Trace::error, e.what());
                MSG(LTFSDMS0074E, e.what());
            }
        }

        return;
    }
};
