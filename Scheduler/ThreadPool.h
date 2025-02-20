#pragma once
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "Thread.h"

class ThreadPool {
public:
    // 线程池执行的任务
    class Task {
    public:
        typedef void(*TaskCallback)(void*); // 回调函数类型
        Task() : mTaskCallback_(NULL), mArg_(NULL) {};

        void setTaskCallback(TaskCallback cb, void* arg) {  // 设置回调函数
            mTaskCallback_ = cb;
            mArg_ = arg;
        }

        void handle() {     // 调用回调函数处理任务
            if(mTaskCallback_)
                mTaskCallback_(mArg_);
        }

        bool operator=(const Task& task) {
            this->mTaskCallback_ = task.mTaskCallback_;
            this->mArg_ = task.mArg_;
        }

    private:
        TaskCallback mTaskCallback_;
        void* mArg_;
    };

    static ThreadPool* createNew(int num);

    explicit ThreadPool(int num);
    ~ThreadPool();

    void addTask(Task& task);

private:
    void loop();

    class MThread : public Thread
    {
    protected:
        virtual void run(void *arg);    // 执行线程的任务
    };

    void createThreads();
    void cancelThreads();

private:
    std::queue<Task> mTaskQueue_;
    std::mutex mMtx_;    // 互斥锁
    std::condition_variable mCon_;   // 条件变量

    std::vector<MThread> mThreads_;  // 线程池中的线程对象
    bool mQuit_;
};