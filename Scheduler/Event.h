#pragma once

typedef void (*EventCallback)(void*);   // 定义了 EventCallback 类型，即一个函数指针，指向接收 void* 参数且无返回值的函数

class TriggerEvent{
public:
    static TriggerEvent* createNew(void* arg);
    static TriggerEvent* createNew();

    TriggerEvent(void* arg);
    ~TriggerEvent();

    void setArg(void* arg) { mArg_ = arg; }
    void setTriggerCallback(EventCallback cb) { mTriggerCallback_ = cb; }
    void handleEvent();

private:
    void* mArg_;
    EventCallback mTriggerCallback_;
};

class TimerEvent{
public:
    static TimerEvent* createNew(void* arg);
    static TimerEvent* createNew();

    TimerEvent(void* arg);
    ~TimerEvent();

    void setArg(void* arg) { mArg_ = arg; }
    void setTimeoutCallback(EventCallback cb) { mTimeoutCallback_ = cb; }
    bool handleEvent();

    void stop();

private:
    void* mArg_;
    EventCallback mTimeoutCallback_;
    bool mIsStop_;
};

class IOEvent{
public:
    enum IOEventType
    {
        EVENT_NONE = 0,
        EVENT_READ = 1,
        EVENT_WRITE = 2,
        EVENT_ERROR = 4,
    };
    
    static IOEvent* createNew(int fd, void* arg);
    static IOEvent* createNew(int fd);

    IOEvent(int fd, void* arg);
    ~IOEvent();

    int getFd() const { return mFd_; }
    int getEvent() const { return mEvent_; }         // 返回监听的事件类型（读、写、错误）
    void setREvent(int event) { mREvent_ = event; }  // 设置实际发生的事件
    void setArg(void* arg) { mArg_ = arg; }

    void setReadCallback(EventCallback cb) { mReadCallback_ = cb; };
    void setWriteCallback(EventCallback cb) { mWriteCallback_ = cb; };
    void setErrorCallback(EventCallback cb) { mErrorCallback_ = cb; };

    void enableReadHandling() { mEvent_ |= EVENT_READ; }
    void enableWriteHandling() { mEvent_ |= EVENT_WRITE; }
    void enableErrorHandling() { mEvent_ |= EVENT_ERROR; }
    void disableReadeHandling() { mEvent_ &= ~EVENT_READ; }
    void disableWriteHandling() { mEvent_ &= ~EVENT_WRITE; }
    void disableErrorHandling() { mEvent_ &= ~EVENT_ERROR; }

    bool isNoneHandling() const { return mEvent_ == EVENT_NONE; }
    bool isReadHandling() const { return (mEvent_ & EVENT_READ) != 0; }
    bool isWriteHandling() const { return (mEvent_ & EVENT_WRITE) != 0; }
    bool isErrorHandling() const { return (mEvent_ & EVENT_ERROR) != 0; };

    void handleEvent();

private:
    int mFd_;
    void* mArg_;
    int mEvent_;
    int mREvent_;
    EventCallback mReadCallback_;
    EventCallback mWriteCallback_;
    EventCallback mErrorCallback_;
};
