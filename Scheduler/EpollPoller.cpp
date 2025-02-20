#include "EpollPoller.h"
#include "../Base/Log.h"

#include "unistd.h"
#include "fcntl.h"

EpollPoller::EpollPoller() {
    mEpollFd_ = epoll_create(0);
    if(mEpollFd_ < 0) {
        LOGE("epoll_create error");
    }
    mEvents_.resize(1024);
}

EpollPoller::~EpollPoller() {
    if(mEpollFd_ >= 0) {
        close(mEpollFd_);
    }
}

EpollPoller* EpollPoller::createNew() {
    return new EpollPoller();
}

bool EpollPoller::addIOEvent(IOEvent* event) {
    return updateIOEvent(event);
}

bool EpollPoller::updateIOEvent(IOEvent* event) {
    int fd = event->getFd();    // 获取事件的fd
    if(fd < 0) {
        return false;
    }

    struct epoll_event ev;
    ev.events = 0;
    ev.data.fd = fd;

    // 将事件类型转换为epoll事件掩码
    if(event->isReadHandling()) {
        ev.events |= EPOLLIN;
    }

    if(event->isWriteHandling()) {
        ev.events |= EPOLLOUT;
    }

    if(event->isErrorHandling()) {
        ev.events |= EPOLLERR;
    }

    int op = EPOLL_CTL_ADD;
    if(mEventMap_.find(fd) != mEventMap_.end()) {   // 如果能找到fd对应事件
        op = EPOLL_CTL_MOD;   // 事件已存在则修改(将当前事件与fd进行映射)
    }

    if(epoll_ctl(mEpollFd_, op, fd, &ev) < 0) {
        return false;
    }

    mEventMap_[fd] = event;
    return true;
}

bool EpollPoller::removeIOEvent(IOEvent* event) {
    int fd = event->getFd();
    if (fd < 0) {
        return false;
    }

    if (epoll_ctl(mEpollFd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        LOGE("Failed to remove fd=%d from epoll", fd);
        return false;
    }

    mEventMap_.erase(fd);
    return true;
}

void EpollPoller::handleEvent() {
    int numEvents = epoll_wait(mEpollFd_, mEvents_.data(), mEvents_.size(), 1000);
    if(numEvents < 0) {
        return;
    }

    for(int i = 0; i < numEvents; ++i) {
        int fd = mEvents_[i].data.fd;
        auto it = mEventMap_.find(fd);  // 在Map中查找fd对应事件
        if(it == mEventMap_.end()) {
            continue;   // 未找到对应的事件
        }

        IOEvent* event = it->second;
        int rEvent = 0;

        // 解析事件类型
        if(mEvents_[i].events & EPOLLIN) {
            rEvent |= IOEvent::EVENT_READ;
        }
        else if(mEvents_[i].events & EPOLLOUT) {
            rEvent |= IOEvent::EVENT_WRITE;
        }
        else if(mEvents_[i].events & EPOLLERR) {
            rEvent |= IOEvent::EVENT_ERROR;
        }

        if(rEvent != 0) {
            event->setREvent(rEvent);   // 设置实际发生的事件
            event->handleEvent();
        }
    }

    // 动态扩容事件数组
    if(numEvents == static_cast<int>(mEvents_.size())) {
        mEvents_.resize(mEvents_.size() * 2);
    }
}
