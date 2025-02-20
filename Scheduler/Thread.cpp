#include "Thread.h"

Thread::Thread() :
    mArg_(nullptr),
    mIsStart_(false),
    mIsDetach_(false)
{

}

Thread::~Thread() {
    if(mIsStart_ && !mIsDetach_) {
        detach();
    }
}

bool Thread::start(void* arg) {
    mArg_ = arg;
    mThreadId = std::thread(&Thread::threadRun, this);

    mIsStart_ = true;

    return true;
}

bool Thread::detach() {
    if(!mIsStart_) {
        return false;
    }
    if(mIsDetach_) {
        return true;
    }

    mThreadId.detach();
    mIsDetach_ = true;

    return true;
}

bool Thread::join() {
    if(!mIsStart_ || mIsDetach_)
        return false;

    mThreadId.join();

    return true;
}

void* Thread::threadRun(void* arg) {
    Thread* thread = (Thread*)arg;
    thread->run(thread->mArg_);

    return NULL;
}