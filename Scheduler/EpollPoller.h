#pragma once
#include "Poller.h"

#include <vector>
#include <sys/epoll.h>
#include <unordered_map>
#include <unistd.h>

class EpollPoller : public Poller {
public:
    EpollPoller();
    ~EpollPoller();

    static EpollPoller* createNew();

    virtual bool addIOEvent(IOEvent* event);
    virtual bool updateIOEvent(IOEvent* event);
    virtual bool removeIOEvent(IOEvent* event);
    virtual void handleEvent();

private:
    int mEpollFd_;
    std::unordered_map<int, IOEvent*> mEventMap_; // 文件描述符到 IOEvent 的映射
    std::vector<struct epoll_event> mEvents_; // 存储 epoll_wait 返回的就绪事件
};