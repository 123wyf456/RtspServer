#include "Timer.h"

#include <sys/timerfd.h>
#include <time.h>
#include <chrono>
#include "Event.h"
#include "EventScheduler.h"
#include "Poller.h"
#include "../Base/Log.h"

static bool timerFdSetTime(int fd, Timer::Timestamp when, Timer::TimeInterval period) {
    struct itimerspec newVal;

    newVal.it_value.tv_sec = when / 1000; //ms->s
    newVal.it_value.tv_nsec = when % 1000 * 1000 * 1000; //ms->ns
    newVal.it_interval.tv_sec = period / 1000;
    newVal.it_interval.tv_nsec = period % 1000 * 1000 * 1000;

    int oldValue = timerfd_settime(fd, TFD_TIMER_ABSTIME, &newVal, NULL);
    if (oldValue < 0) {
        return false;
    }
    return true;
}

// 定时器
Timer::Timer(TimerEvent* event, Timestamp timestamp, TimeInterval timeInterval, TimerId timerId) :
        mTimerEvent_(event),
        mTimestamp_(timestamp),
        mTimeInterval_(timeInterval),
        mTimerId_(timerId){
    if (timeInterval > 0){
        mRepeat_ = true;// 循环定时器
    }else{
        mRepeat_ = false;//一次性定时器
    }
}

Timer::~Timer()
{

}

Timer::Timestamp Timer::getCurTime() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec*1000 + now.tv_nsec/1000000);
}

Timer::Timestamp Timer::getCurTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool Timer::handleEvent() {
    if(!mTimerEvent_) {
        return false;
    }
    return mTimerEvent_->handleEvent();
}

// 定时器管理
TimerManager* TimerManager::createNew(EventScheduler* scheduler){

    if(!scheduler)
        return NULL;
    return new TimerManager(scheduler);
}

TimerManager::TimerManager(EventScheduler* scheduler) :
        mPoller_(scheduler->poller()),
        mLastTimerId_(0)
{
    mTimerFd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);    // 创建定时器fd
    if (mTimerFd_ < 0) {
        LOGE("create TimerFd error");
        return;
    }else{
        LOGI("fd=%d",mTimerFd_);
    }

    mTimerIOEvent_ = IOEvent::createNew(mTimerFd_, this);   // 创建定时器IO事件
    mTimerIOEvent_->setReadCallback(readCallback);          // 设置读回调
    mTimerIOEvent_->enableReadHandling();                   // 设置为可读
    modifyTimeout();
    mPoller_->addIOEvent(mTimerIOEvent_);                   // 添加定时器IO事件到poller
}

TimerManager::~TimerManager() {
    mPoller_->removeIOEvent(mTimerIOEvent_);
    delete mTimerIOEvent_;
}

Timer::TimerId TimerManager::addTimer(TimerEvent* event, Timer::Timestamp timestamp, Timer::TimeInterval timeinterval) {
    ++mLastTimerId_;
    Timer timer(event, timestamp, timeinterval, mLastTimerId_);

    mTimers_.insert(std::make_pair(mLastTimerId_, timer));
    mEvents_.insert(std::make_pair(timestamp, timer));
    modifyTimeout();

    return mLastTimerId_;
}

bool TimerManager::removeTimer(Timer::TimerId timerId)
{
    std::map<Timer::TimerId, Timer>::iterator it = mTimers_.find(timerId);
    if(it != mTimers_.end())
    {
        mTimers_.erase(timerId);
        // TODO 还需要删除mEvents的事件
    }

    modifyTimeout();

    return true;
}

void TimerManager::modifyTimeout() {
    std::multimap<Timer::Timestamp, Timer>::iterator it = mEvents_.begin();
    if(it != mEvents_.end()) {  // 存在至少一个定时器
        Timer timer = it->second;
        timerFdSetTime(mTimerFd_, timer.mTimestamp_, timer.mTimeInterval_);
    }
    else {
        timerFdSetTime(mTimerFd_, 0, 0);
    }
}

void TimerManager::readCallback(void *arg) {
    TimerManager* timerManager = (TimerManager*)arg;
    timerManager->handleRead();
}

void TimerManager::handleRead() {
    Timer::Timestamp timestamp = Timer::getCurTime();
    if(!mTimers_.empty() && !mEvents_.empty()) {
        std::multimap<Timer::Timestamp, Timer>::iterator it = mEvents_.begin();     // 取出最早的定时任务
        Timer timer = it->second;   // 获取Timer对象
        int expire = timer.mTimestamp_ - timestamp;

        if(timestamp > timer.mTimestamp_ || expire == 0) {
            bool timerEventIsStop = timer.handleEvent();
            mEvents_.erase(it);

            // 如果是周期任务，重新计算下一次触发时间，并重新加入队列
            if (timer.mRepeat_) {
                if (timerEventIsStop) {
                    mTimers_.erase(timer.mTimerId_);
                }else {
                    timer.mTimestamp_ = timestamp + timer.mTimeInterval_;
                    mEvents_.insert(std::make_pair(timer.mTimestamp_, timer));
                }
            }
            else {
                mTimers_.erase(timer.mTimerId_);
            }
        }
    }

    modifyTimeout();    // 处理完当前定时器任务后，更新下一个定时器的超时时间
}