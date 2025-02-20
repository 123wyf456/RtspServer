#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <stdint.h>

#include "Event.h"
#include "Timer.h"
#include "EpollPoller.h"

class Poller;

class EventScheduler {
public:
    enum PollerType {
        SELECT,
        POLL,
        EPOLL
    };

    static EventScheduler* createnew(PollerType type);
    explicit EventScheduler(PollerType type);
    virtual ~EventScheduler();

public:
    // 触发事件
    bool addTriggerEvent(TriggerEvent* event);

    // 定时器事件
    Timer::TimerId addTimerEventRunAfter(TimerEvent* event, Timer::TimeInterval delay);
    Timer::TimerId addTimerEventRunAt(TimerEvent* event, Timer::Timestamp when);
    Timer::TimerId addTimerEventRunEvery(TimerEvent* event, Timer::TimeInterval interval);

    // IO事件
    bool addIOEvent(IOEvent* event);
    bool updateIOEvent(IOEvent* event);
    bool removeIOEvent(IOEvent* event);

    void loop();
    Poller* poller();
    void setTimerManagerReadCallback(EventCallback* cb, void* arg);

private:
    void handleTriggerEvents();

private:
    bool mQuit_;
    Poller* mPoller_;
    TimerManager* mTimerManager_;
    std::vector<TriggerEvent*> mTriggerEvents_;

    std::mutex mMtx_;

    // 定时器回调（Windows）
    EventCallback mTimerManagerReadCallback_;
    void* mTimerManagerArg_;
};