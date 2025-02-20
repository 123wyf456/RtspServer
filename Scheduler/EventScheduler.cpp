#include "EventScheduler.h"
#include "../Base/Log.h"
#include "SocketOps.h"
#include "EpollPoller.h"

#include <sys/eventfd.h>

EventScheduler* EventScheduler::createnew(PollerType type) {
    if(type != SELECT && type != POLL && type != EPOLL)
        return NULL;
    return new EventScheduler(type);
}

EventScheduler::EventScheduler(PollerType type) :
    mQuit_(false)
{
    switch (type)
    {
    case SELECT:
        break;
    case POLL:
        break;    
    case EPOLL:
        mPoller_ = EpollPoller::createNew();
        break;    
    default:
        LOGE("Unknown poller type");
        _exit(-1);
        break;
    }

    mTimerManager_ = TimerManager::createNew(this);
}

EventScheduler::~EventScheduler() {
    delete mTimerManager_;
    delete mPoller_;
}

// 触发事件
bool EventScheduler::addTriggerEvent(TriggerEvent* event) {
    mTriggerEvents_.push_back(event);
    return true;
}

// 定时器事件
Timer::TimerId EventScheduler::addTimerEventRunAfter(TimerEvent* event, Timer::TimeInterval delay) {
    Timer::Timestamp timestamp = Timer::getCurTime();
    timestamp += delay;

    return mTimerManager_->addTimer(event, timestamp, 0);
}

Timer::TimerId EventScheduler::addTimerEventRunAt(TimerEvent* event, Timer::Timestamp when) {
    return mTimerManager_->addTimer(event, when, 0);
}

Timer::TimerId EventScheduler::addTimerEventRunEvery(TimerEvent* event, Timer::TimeInterval interval) {
    Timer::Timestamp timestamp = Timer::getCurTime();
    timestamp += interval;

    return mTimerManager_->addTimer(event, timestamp, interval);
}

// IO事件
bool EventScheduler::addIOEvent(IOEvent* event) {
    return mPoller_->addIOEvent(event);
}

bool EventScheduler::updateIOEvent(IOEvent* event) {
    return mPoller_->updateIOEvent(event);
}

bool EventScheduler::removeIOEvent(IOEvent* event) {
    return mPoller_->removeIOEvent(event);
}

void EventScheduler::loop() {
    while(!mQuit_) {
        handleTriggerEvents();  // 如果有触发事件，处理触发事件
        mPoller_->handleEvent();    // 通过 Poller 监听事件，当事件发生时，调用相关事件的回调函数来处理
    }
}

void EventScheduler::handleTriggerEvents()
{
    if (!mTriggerEvents_.empty())
    {
        for (std::vector<TriggerEvent*>::iterator it = mTriggerEvents_.begin();
             it != mTriggerEvents_.end(); ++it)
        {
            (*it)->handleEvent();
        }

        mTriggerEvents_.clear();
    }
}

Poller* EventScheduler::poller() {
    return mPoller_;
}