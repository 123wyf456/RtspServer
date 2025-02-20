#include <stdio.h>

#include "Event.h"
#include "../Base/Log.h"

// 触发事件
TriggerEvent* TriggerEvent::createNew(void* arg){
    return new TriggerEvent(arg);
}

TriggerEvent* TriggerEvent::createNew(){
    return new TriggerEvent(NULL);
}

TriggerEvent::TriggerEvent(void* arg) : mArg_(arg), mTriggerCallback_(NULL){
    LOGI("TriggerEvent()");
}
TriggerEvent::~TriggerEvent() {
    LOGI("~TriggerEvent()");

}

void TriggerEvent::handleEvent(){
    if(mTriggerCallback_)
        mTriggerCallback_(mArg_);
}

// 定时器事件
TimerEvent* TimerEvent::createNew(void* arg){
    return new TimerEvent(arg);
}

TimerEvent* TimerEvent::createNew(){
    return new TimerEvent(NULL);
}

TimerEvent::TimerEvent(void* arg) : mArg_(arg), 
mTimeoutCallback_(NULL),
mIsStop_(false){
    LOGI("TimerEvent()");
}

TimerEvent::~TimerEvent() {
    LOGI("~TimerEvent()");
}

bool TimerEvent::handleEvent()
{
    if (mIsStop_) {
        return mIsStop_;
    }

    if(mTimeoutCallback_)
        mTimeoutCallback_(mArg_);

    return mIsStop_;
}

void TimerEvent::stop() {
    mIsStop_ = true;
}

// IO事件
IOEvent* IOEvent::createNew(int fd, void* arg){
    if(fd < 0)
        return NULL;

    return new IOEvent(fd, arg);
}

IOEvent* IOEvent::createNew(int fd){
    if(fd < 0)
        return NULL;
   
    return new IOEvent(fd, NULL);
}

IOEvent::IOEvent(int fd, void* arg) :
    mFd_(fd),
    mArg_(arg),
    mEvent_(EVENT_NONE),
    mREvent_(EVENT_NONE),
    mReadCallback_(NULL),
    mWriteCallback_(NULL),
    mErrorCallback_(NULL){

    LOGI("IOEvent() fd=%d",mFd_);
}
IOEvent::~IOEvent() {
    LOGI("~IOEvent() fd=%d", mFd_);
}

void IOEvent::handleEvent()
{
    if (mReadCallback_ && (mREvent_ & EVENT_READ))
    {
        mReadCallback_(mArg_);
    }

    if (mWriteCallback_ && (mREvent_ & EVENT_WRITE))
    {
        mWriteCallback_(mArg_);
    }
    
    if (mErrorCallback_ && (mREvent_ & EVENT_ERROR))
    {
        mErrorCallback_(mArg_);
    }
};
