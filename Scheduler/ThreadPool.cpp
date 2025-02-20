#include "ThreadPool.h"
#include "../Base/Log.h"

ThreadPool* ThreadPool::createNew(int num)
{
    return new ThreadPool(num);
}

ThreadPool::ThreadPool(int num) :
    mThreads_(num),
    mQuit_(false)
{
    createThreads();
}

ThreadPool::~ThreadPool()
{
    cancelThreads();
}

void ThreadPool::addTask(ThreadPool::Task& task) {
    std::unique_lock<std::mutex> lck(mMtx_);
    mTaskQueue_.push(task);
    mCon_.notify_one();
}

void ThreadPool::loop() {
    while(!mQuit_) {
        std::unique_lock<std::mutex> lck(mMtx_);
        if(mTaskQueue_.empty()) {
            mCon_.wait(lck);
        }

        if(mTaskQueue_.empty()) {
            continue;
        }

        Task task = mTaskQueue_.front();
        mTaskQueue_.pop();
        task.handle();
    }
}

void ThreadPool::createThreads() {
    std::unique_lock<std::mutex> lck(mMtx_);
    for(auto& mThread : mThreads_) {
        mThread.start(this);
    }
}

void ThreadPool::cancelThreads() {
    std::unique_lock<std::mutex> lck(mMtx_);
    mQuit_ = true;
    mCon_.notify_all();
    for(auto& mThrad : mThreads_) {
        mThrad.join();
    }

    mThreads_.clear();
}

void ThreadPool::MThread::run(void* arg)
{
    ThreadPool* threadPool = (ThreadPool*)arg;
    threadPool->loop();
}