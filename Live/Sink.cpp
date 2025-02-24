#include "Sink.h"
#include "../Scheduler/SocketOps.h"
#include "../Base/Log.h"

Sink::Sink(UsageEnvironment* env, MediaSource* MediaSource, int payloadType) :
    mMediaSource(MediaSource),
    mEnv(env),
    mCsrcLen(0),
    mExtension(0),
    mPadding(0),
    mVersion(RTP_VERSION),
    mPayloadType(payloadType),
    mMarker(0),
    mSeq(0),
    mTimestamp(0),
    mSSRC(0)
{
    LOGI("SINK()");
    mTimerEvent = TimerEvent::createNew(this);
    mTimerEvent->setTimeoutCallback(cbTimeout);
}

Sink::~Sink() {
    LOGI("~Sink()");
    delete mTimerEvent;
    delete mMediaSource;
}

void Sink::stopTimerEvent() {
    mTimerEvent->stop();
}

void Sink::setSessionCb(SessionSendPacketCallback cb, void* arg1, void* arg2) {
    // cb 被回调函数
    // arg1 mediaSession 对象指针
    // arg2 mediaSession 被回调track对象指针
    mSessionSendPacket = cb;
    mArg1 = arg1;
    mArg2 = arg2;
}

void Sink::sendRtpPacket(RtpPacket* packet) {
    RtpHeader* rtpHeader = packet->mRtpHeader;  // 初始化一个RTPHeader对象
    rtpHeader->csrcLen = mCsrcLen;
    rtpHeader->extension = mExtension;
    rtpHeader->padding = mPadding;
    rtpHeader->version = mVersion;
    rtpHeader->payloadType = mPayloadType;
    rtpHeader->marker = mMarker;
    rtpHeader->seq = mSeq;
    rtpHeader->timestamp = mTimestamp;
    rtpHeader->ssrc = mSSRC;

    if(mSessionSendPacket) {
        mSessionSendPacket(mArg1, mArg2, packet, PacketType::RTPPACKET);
    }
}

void Sink::runEvery(int interval) {
    mTimerId = mEnv->scheduler()->addTimerEventRunEvery(mTimerEvent, interval);
}

void Sink::cbTimeout(void* arg) {
    Sink* sink = (Sink*)arg;
    sink->handleTimeout();
}

void Sink::handleTimeout() {
    MediaFrame* frame = mMediaSource->getFrameFromOutputQueue();
    if(!frame) {
        return;
    }

    this->sendFrame(frame);

    // 将使用过的frame插入输入队列，插入输入队列以后，加入一个子线程task，从文件中读取数据再次将输入写入到frame
    mMediaSource->putFrameToInputQueue(frame);  // InputQueue起到一个缓存区的作用
}