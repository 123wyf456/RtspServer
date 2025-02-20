#pragma once
#include <thread>

class Thread {
public:
    virtual ~Thread();

    bool start(void* arg);
    bool detach();
    bool join();

protected:
    Thread();

    virtual void run(void* arg) = 0;

private:
    static void* threadRun(void*);

private:
    void* mArg_;
    bool mIsStart_;
    bool mIsDetach_;
    std::thread mThreadId;
};