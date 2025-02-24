#include "MediaSession.h"
#include "../Base/Log.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <algorithm>
#include <assert.h>

MediaSession* MediaSession::createNew(std::string sessionName) {
    return new MediaSession(sessionName);
}

MediaSession::MediaSession(std::string sessionName) :
    mSessionName(sessionName),
    mIsStartMulticast(false)
{
    LOGI("MediaSession() name=%s",sessionName.data());

    // 初始化两个track，默认未激活
    mTracks[0].mTrackId = TrackId0;
    mTracks[0].mIsAlive = false;

    mTracks[1].mTrackId = TrackId1;
    mTracks[1].mIsAlive = false;

    // 初始化多播RTP实例为空
    for(int i = 0; i < MEDIA_MAX_TRACK_NUM; i++) {
        mMulticastRtpInstances[i] = NULL;
        mMulticastRtcpInstances[i] = NULL;
    }
}

MediaSession::~MediaSession() {
    LOGI("~MediaSession()");
    for(int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i)
    {
        // 删除多播 RTP 实例
        if(mMulticastRtpInstances[i])
        {
            this->removeRtpInstance(mMulticastRtpInstances[i]);
            delete mMulticastRtpInstances[i];
        }
    
        // 删除多播 RTCP 实例
        if (mMulticastRtcpInstances[i]) {
            delete mMulticastRtcpInstances[i];
        }
    }

    // 删除 Sink（数据生产者）
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i)
    {
        if (mTracks[i].mIsAlive) {
            Sink* sink = mTracks[i].mSink;
            delete sink;
        }
    }
}

std::string MediaSession::generateSDPDescription() {
    if(!mSdp.empty())
    return mSdp;

    std::string ip = "0.0.0.0";
    char buf[2048] = {0};

    // SDP 基本信息
    snprintf(buf, sizeof(buf),
        "v=0\r\n"
        "o=- 9%ld 1 IN IP4 %s\r\n"
        "t=0 0\r\n"
        "a=control:*\r\n"
        "a=type:broadcast\r\n",
        (long)time(NULL), ip.c_str());

    if(isStartMulticast())
    {
        /*
        * buf + strlen(buf) 计算 buf 当前已使用的长度，确保新的内容被追加到已有内容的末尾，而不会覆盖之前的数据。
        * sizeof(buf) - strlen(buf) 计算 buf 剩余可用空间，确保不会发生缓冲区溢出。
        */
        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                "a=rtcp-unicast: reflection\r\n");
    }

    // 逐个 Track 生成 SDP
    for(int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i)
    {
        uint16_t port = 0;

        if(mTracks[i].mIsAlive != true)
            continue;

        if(isStartMulticast())
            port = getMulticastDestRtpPort((TrackId)i); // 获取轨道的多播RTP端口
        
        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                    "%s\r\n", mTracks[i].mSink->getMediaDescription(port).c_str()); // 获取当前 Track 的媒体描述信息，并追加到 SDP 缓冲区

        if(isStartMulticast())
            snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                        "c=IN IP4 %s/255\r\n", getMulticastDestAddr().c_str());
        else
            snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                        "c=IN IP4 0.0.0.0\r\n");    // 非多播模式下，地址默认为 0.0.0.0

        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                    "%s\r\n", mTracks[i].mSink->getAttribute().c_str());    // 获取当前 Track 的媒体属性信息，并追加到 SDP 缓冲区

        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),											
                "a=control:track%d\r\n", mTracks[i].mTrackId);  // 设置 Track 控制 URL
    }

    mSdp = buf;
    return mSdp;
}

// 向轨道加入数据生产者
bool MediaSession::addSink(MediaSession::TrackId trackid, Sink* sink) {
    Track* track = getTrack(trackid);

    if(!track)
        return false;

    track->mSink = sink;
    track->mIsAlive = true;

    sink->setSessionCb(MediaSession::sendPacketCallback, this, track);
    return true;
}

// 向轨道加入RTP实例（消费者）
bool MediaSession::addRtpInstance(MediaSession::TrackId trackid, RtpInstance* rtpInstance) {
    Track* track = getTrack(trackid);

    if(!track)
        return false;

    track->mRtpInstances.push_back(rtpInstance);

    return true;
}

bool MediaSession::removeRtpInstance(RtpInstance* rtpInstance) {
    for(int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        if(mTracks[i].mIsAlive == false)
            continue;

        std::list<RtpInstance*>::iterator it = std::find(mTracks[i].mRtpInstances.begin(), mTracks[i].mRtpInstances.end(), rtpInstance);
        if(it == mTracks[i].mRtpInstances.end())
            continue;
        
        mTracks[i].mRtpInstances.erase(it);
        return true;
    }

    return false;
}

MediaSession::Track* MediaSession::getTrack(MediaSession::TrackId trackId) {
    for(int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        if(mTracks[i].mTrackId == trackId) {
            return &mTracks[i];
        }
    }

    return NULL;
}

void MediaSession::sendPacketCallback(void* arg1, void* arg2, void* packet, Sink::PacketType packetType) {
    RtpPacket* rtpPacket = (RtpPacket*)packet;  // 要发送的RTP包数据

    MediaSession* session = (MediaSession*)arg1;    // 进行数据转发的会话
    MediaSession::Track* track = (MediaSession::Track*)arg2;    // 传输数据的轨道
    
    session->handleSendRtpPacket(track, rtpPacket);     // 处理RTP数据包发送
}

void MediaSession::handleSendRtpPacket(MediaSession::Track* track, RtpPacket* packet) {
    std::list<RtpInstance*>::iterator it;   // 声明一个迭代器，用于遍历 track 中的所有 RtpInstance 对象

    // 遍历 track->mRtpInstances（一个存储 RtpInstance 指针的列表）
    for(it = track->mRtpInstances.begin(); it != track->mRtpInstances.end(); ++it){
        RtpInstance* rtpInstance = *it; // 获取当前迭代器所指向的 RtpInstance 对象
        if (rtpInstance->alive()){
            rtpInstance->send(packet);   // 通过 RtpInstance 对象发送RTP包数据
        }
    }
}

bool MediaSession::startMulticast()
{
    // 随机生成多播地址
    struct sockaddr_in addr = { 0 };
    uint32_t range = 0xE8FFFFFF - 0xE8000100;
    addr.sin_addr.s_addr = htonl(0xE8000100 + (rand()) % range);
    mMulticastAddr = inet_ntoa(addr.sin_addr);

    // 创建 Sockets 类的实例
    Sockets rtpSocket1(0), rtcpSocket1(0);
    Sockets rtpSocket2(0), rtcpSocket2(0);

    // 端口号
    uint16_t rtpPort1, rtcpPort1;
    uint16_t rtpPort2, rtcpPort2;
    bool ret;

    // 创建UDP套接字
    int rtpSockfd1 = rtpSocket1.CreateUdpSock();
    assert(rtpSockfd1 > 0);

    int rtpSockfd2 = rtpSocket2.CreateUdpSock();
    assert(rtpSockfd2 > 0);

    int rtcpSockfd1 = rtcpSocket1.CreateUdpSock();
    assert(rtcpSockfd1 > 0);

    int rtcpSockfd2 = rtcpSocket2.CreateUdpSock();
    assert(rtcpSockfd2 > 0);

    uint16_t port = rand() & 0xfffe;
    if(port < 10000)
        port += 10000;

    rtpPort1 = port;
    rtcpPort1 = port+1;
    rtpPort2 = rtcpPort1+1;
    rtcpPort2 = rtpPort2+1;

    mMulticastRtpInstances[TrackId0] = RtpInstance::createNewOverUdp(rtpSockfd1, 0, mMulticastAddr, rtpPort1);
    mMulticastRtpInstances[TrackId1] = RtpInstance::createNewOverUdp(rtpSockfd2, 0, mMulticastAddr, rtpPort2);
    mMulticastRtcpInstances[TrackId0] = RtcpInstance::createNew(rtcpSockfd1, 0, mMulticastAddr, rtcpPort1);
    mMulticastRtcpInstances[TrackId1] = RtcpInstance::createNew(rtcpSockfd2, 0, mMulticastAddr, rtcpPort2);

    this->addRtpInstance(TrackId0, mMulticastRtpInstances[TrackId0]);
    this->addRtpInstance(TrackId1, mMulticastRtpInstances[TrackId1]);
    mMulticastRtpInstances[TrackId0]->setAlive(true);
    mMulticastRtpInstances[TrackId1]->setAlive(true);

    mIsStartMulticast = true;

    return true;
}

bool MediaSession::isStartMulticast()
{
    return mIsStartMulticast;
}

uint16_t MediaSession::getMulticastDestRtpPort(TrackId trackId)
{
    if(trackId > TrackId1 || !mMulticastRtpInstances[trackId])
        return -1;

    return mMulticastRtpInstances[trackId]->getPeerPort();
}