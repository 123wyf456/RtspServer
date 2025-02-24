#pragma once
#include <string>
#include <stdint.h>

#include "Rtp.h"
#include "MediaSource.h"
#include "../Scheduler/Event.h"
#include "../Scheduler/UsageEnvironment.h"

class Sink {
public:
    enum PacketType {
        UNKNOWN = -1,
        RTPPACKET = 0,
    };

    typedef void (*SessionSendPacketCallback)(void* arg1, void* arg2, void* packet, PacketType packettype);

    Sink(UsageEnvironment* env, MediaSource* MediaSource, int payloadType);
    virtual ~Sink();

    void stopTimerEvent();   // 停止定时器事件

    virtual std::string getMediaDescription(uint16_t port) = 0;
    virtual std::string getAttribute() = 0;

    void setSessionCb(SessionSendPacketCallback cb, void* arg1, void* arg2);    // 发送包的回调函数

protected:
    virtual void sendFrame(MediaFrame* frame) = 0;
    void sendRtpPacket(RtpPacket* rtppacket);

    void runEvery(int interval);

private:
    static void cbTimeout(void* arg);
    void handleTimeout();

protected:
    UsageEnvironment* mEnv;
    MediaSource* mMediaSource;
    SessionSendPacketCallback mSessionSendPacket;
    void* mArg1;
    void* mArg2;

    // RtpHeader参数
    uint8_t mCsrcLen;
    uint8_t mExtension;
    uint8_t mPadding;
    uint8_t mVersion;
    uint8_t mPayloadType;
    uint8_t mMarker;
    uint16_t mSeq;
    uint32_t mTimestamp;
    uint32_t mSSRC;  

private:
    TimerEvent* mTimerEvent;
    Timer::TimerId mTimerId;

};