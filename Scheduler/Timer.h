#pragma once

#include <stdint.h>
#include <map>

class EventScheduler;
class Poller;
class TimerEvent;
class IOEvent;

class Timer {
public:
    typedef uint32_t TimerId;
    typedef int64_t Timestamp;
    typedef uint32_t TimeInterval;

    ~Timer();

    static Timestamp getCurTime();
    static Timestamp getCurTimestamp();

private:
    friend class TimerManager;
    Timer(TimerEvent* event, Timestamp timestamp, TimeInterval timeinterval, TimerId timeid);

private:
    bool handleEvent();

private:
    TimerEvent* mTimerEvent_;

    Timestamp mTimestamp_;
    TimeInterval mTimeInterval_;
    TimerId mTimerId_;

    bool mRepeat_;
};

class TimerManager {
public:
    static TimerManager* createNew(EventScheduler* scheduler);

    TimerManager(EventScheduler* scheduler);
    ~TimerManager();

    Timer::TimerId addTimer(TimerEvent* event, Timer::Timestamp timestamp, Timer::TimeInterval timeinterval);
    bool removeTimer(Timer::TimerId timerid);

private:
    static void readCallback(void* arg);
    void handleRead();
    void modifyTimeout();

private:
    Poller* mPoller_;
    std::map<Timer::TimerId, Timer> mTimers_;           // 存储所有活动的定时任务
    std::multimap<Timer::Timestamp, Timer> mEvents_;    // 按时间顺序管理即将到期的定时任务
    uint32_t mLastTimerId_;

    int mTimerFd_;
    IOEvent* mTimerIOEvent_;

};