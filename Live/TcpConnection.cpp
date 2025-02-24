#include "TcpConnection.h"
#include "../Base/Log.h"

TcpConnection::TcpConnection(UsageEnvironment* env, int clientFd) :
    mEnv(env),
    mClientFd(clientFd)
{
    mClientIOEvent = IOEvent::createNew(clientFd, this);
    mClientIOEvent->setReadCallback(readCallback);
    mClientIOEvent->setWriteCallback(writeCallback);
    mClientIOEvent->setErrorCallback(errorCallback);
    mClientIOEvent->enableReadHandling();

    mEnv->scheduler()->addIOEvent(mClientIOEvent);
}

TcpConnection::~TcpConnection() {
    mEnv->scheduler()->removeIOEvent(mClientIOEvent);
    delete mClientIOEvent;
    close(mClientFd);
}

void TcpConnection::setDisConnectCallback(DisConnectCallback cb, void* arg) {
    mDisConnectCallback = cb;
    mArg = arg;
}

void TcpConnection::enableReadHanding() {
    if(mClientIOEvent->isReadHandling())
        return;
    
    mClientIOEvent->enableReadHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::enableWriteHanding() {
    if(mClientIOEvent->isWriteHandling())
        return;
    
    mClientIOEvent->enableWriteHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::enableErrorHanding() {
    if(mClientIOEvent->isErrorHandling())
        return;

    mClientIOEvent->enableErrorHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::disableReadHanding() {
    if(!mClientIOEvent->isReadHandling())
        return;

    mClientIOEvent->disableReadeHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::disableWriteHanding() {
    if (!mClientIOEvent->isWriteHandling())
        return;

    mClientIOEvent->disableWriteHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::disableErrorHanding() {
    if (!mClientIOEvent->isErrorHandling())
        return;

    mClientIOEvent->disableErrorHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::handleRead() {
    int ret = mInputBuffer.read(mClientFd); // 从客户端读取数据到输入缓冲区
    if(ret <= 0) {
        LOGE("read error, fd=%d, ret=%d", mClientFd, ret);
        handleDisConnect();
        return;
    }

    handleReadBytes();
}

void TcpConnection::handleReadBytes() {
    LOGI("");
    mInputBuffer.retrieveAll();
}

void TcpConnection::handleWrite() {
    LOGI("");
    mOutputBuffer.retrieveAll();
}

void TcpConnection::handleError() {
    LOGI("");
}

void TcpConnection::handleDisConnect() {
    if (mDisConnectCallback) {
        mDisConnectCallback(mArg, mClientFd);
    }
}

void TcpConnection::readCallback(void* arg) {
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleRead();
}

void TcpConnection::writeCallback(void* arg) {
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleWrite();
}

void TcpConnection::errorCallback(void* arg) {
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleError();
}