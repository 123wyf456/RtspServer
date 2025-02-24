#include "RtspServer.h"
#include "RtspConnection.h"
#include "../Base/Log.h"

Sockets* mSocket = new Sockets(0);

RtspServer* RtspServer::createNew(UsageEnvironment* env, MediaSessionManager * sessMgr, Ipv4Address& addr) {
    return new RtspServer(env, sessMgr, addr);
}

RtspServer::RtspServer(UsageEnvironment* env, MediaSessionManager* sessMgr, Ipv4Address& addr) :
    mEnv(env),
    mSessMgr(sessMgr),
    mAddr(addr),
    mListen(false)
{

    mFd = mSocket->CreateTcpSock();
    mSocket->setReuseAddr(1);

    if(!mSocket->bind(addr.getIp(), addr.getPort())) {    // 绑定地址和端口
        return;
    }

    LOGI("rtsp://%s:%d fd=%d",addr.getIp().data(),addr.getPort(), mFd);

    mAcceptIOEvent = IOEvent::createNew(mFd, this);
    mAcceptIOEvent->setReadCallback(readCallback);
    mAcceptIOEvent->enableReadHandling();

    mCloseTriggerEvent = TriggerEvent::createNew(this);
    mCloseTriggerEvent->setTriggerCallback(cbCloseConnect);
}

RtspServer::~RtspServer() {
    if(mListen)
        mEnv->scheduler()->removeIOEvent(mAcceptIOEvent);
    
    delete mAcceptIOEvent;
    delete mCloseTriggerEvent;

    ::close(mFd);
}

void RtspServer::start() {
    LOGI("");
    mListen = true;
    mSocket->listen(60);
    mEnv->scheduler()->addIOEvent(mAcceptIOEvent);
}

void RtspServer::readCallback(void* arg) {
    RtspServer* rtspServer = (RtspServer*)arg;
    rtspServer->handleRead();
}

void RtspServer::handleRead() {
    int clientFd = mSocket->accept();
    if(clientFd < 0) {
        LOGE("handleRead error, clientFd = %d", clientFd);
        return;
    }
    RtspConnection* conn = RtspConnection::createNew(this, clientFd);
    conn->setDisConnectCallback(RtspServer::cbDisConnect, this);
    mConnMap.insert(std::make_pair(clientFd, conn));
}

void RtspServer::cbDisConnect(void* arg, int clientFd) {
    RtspServer* server = (RtspServer*)arg;
    server->handleDisConnect(clientFd);
}

void RtspServer::handleDisConnect(int clientFd) {
    std::lock_guard<std::mutex> lck(mMtx);
    mDisConnList.push_back(clientFd);   // 将客户端加入到断开连接list，由handleCloseConnect进行删除关闭

    mEnv->scheduler()->addTriggerEvent(mCloseTriggerEvent);
}

void RtspServer::cbCloseConnect(void* arg) {
    RtspServer* server = (RtspServer*)arg;
    server->handleCloseConnect();
}

void RtspServer::handleCloseConnect() {
    std::lock_guard <std::mutex> lck(mMtx);

    // 遍历断开连接list，删除关闭客户端
    for(std::vector<int>::iterator it = mDisConnList.begin(); it != mDisConnList.end(); ++it) {
        int clientFd = *it;
        std::map<int, RtspConnection*>::iterator _it = mConnMap.find(clientFd);
        assert(_it != mConnMap.end());
        delete _it->second;
        mConnMap.erase(clientFd);
    }

    mDisConnList.clear();   // 清空断开连接的list
}