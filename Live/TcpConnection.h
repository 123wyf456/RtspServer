#pragma once

#include "../Scheduler/UsageEnvironment.h"
#include "../Scheduler/Event.h"
#include "Buffer.h"

class TcpConnection {
public:
    typedef void (*DisConnectCallback)(void*, int);

    TcpConnection(UsageEnvironment* env, int clientFd);
    virtual ~TcpConnection();

    void setDisConnectCallback(DisConnectCallback cb, void* arg);

protected:
    // 注册事件
    void enableReadHanding();
    void enableWriteHanding();
    void enableErrorHanding();

    // 禁用事件处理
    void disableReadHanding();
    void disableWriteHanding();
    void disableErrorHanding();

    void handleRead();
    virtual void handleReadBytes();
    virtual void handleWrite();
    virtual void handleError();

    void handleDisConnect();

private:
    static void readCallback(void* arg);
    static void writeCallback(void* arg);
    static void errorCallback(void* arg);

protected:
    UsageEnvironment* mEnv;
    int mClientFd;
    IOEvent* mClientIOEvent;
    DisConnectCallback mDisConnectCallback;
    void* mArg;
    Buffer mInputBuffer;
    Buffer mOutputBuffer;
    char mBuffer[2048];

};