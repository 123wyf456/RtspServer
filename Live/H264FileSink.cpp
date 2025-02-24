#include "H264FileSink.h"
#include "../Base/Log.h"
#include <stdio.h>
#include <string.h>

H264FileSink* H264FileSink::createNew(UsageEnvironment* env, MediaSource* mediasource) {
    return new H264FileSink(env, mediasource);
}

H264FileSink::H264FileSink(UsageEnvironment* env, MediaSource* mediasource) :
    Sink(env, mediasource, RTP_PAYLOAD_TYPE_H264),
    mClockRate(90000),
    mFps(mediasource->getFps())
{
    LOGI("H264FileSink()");
    runEvery(1000 / mFps);
}

H264FileSink::~H264FileSink() {
    LOGI("~H264FileSink()");
}

std::string H264FileSink::getMediaDescription(uint16_t port) {
    char buf[100] = {0};
    sprintf(buf, "m=video %hu RTP/AVP %d", port, mPayloadType);

    return std::string(buf);
}

std::string H264FileSink::getAttribute() {
    char buf[100] = {0};
    sprintf(buf, "a=rtpmap:%d H264/%d\r\n", mPayloadType, mClockRate);
    sprintf(buf + strlen(buf), "a=framerate:%d", mFps);

    return std::string(buf);
}

void H264FileSink::sendFrame(MediaFrame* frame) {
    RtpHeader* rtpHeader = mRtpPacket.mRtpHeader;
    uint8_t naluType = frame->mBuf[0];

    // 单一NALU单元模式
    if(frame->mSize <= RTP_MAX_RTP_SIZE) {
        memcpy(rtpHeader->payload, frame->mBuf, frame->mSize);
        mRtpPacket.mSize = RTP_HEADER_SIZE + frame->mSize;
        sendRtpPacket(&mRtpPacket);
        mSeq++;

        if((naluType & 0x1F) == 7 || (naluType & 0x1F) == 8)     // 如果是SPS、PPS就不需要加时间戳
            return;
    }
    // 分片模式
    else {
        int pktNum = frame->mSize / RTP_MAX_RTP_SIZE;
        int remainPktSize = frame->mSize % RTP_MAX_RTP_SIZE;
        int i, pos = 1;

        // 发送完整的包
        for(i = 0; i < pktNum; ++i) {
            rtpHeader->payload[0] = (naluType & 0x60) | 28;
            rtpHeader->payload[1] = naluType & 0x1F;

            if(i == 0) {
                rtpHeader->payload[1] |= 0x80;
            }
            else if(remainPktSize == 0 && i == pktNum - 1) {
                rtpHeader->payload[1] |= 0x40;
            }

            memcpy(rtpHeader->payload + 2, frame->mBuf + pos, RTP_MAX_RTP_SIZE);
            mRtpPacket.mSize = RTP_HEADER_SIZE + 2 + RTP_MAX_RTP_SIZE;
            sendRtpPacket(&mRtpPacket);

            mSeq++;
            pos += RTP_MAX_RTP_SIZE;
        }

        // 发送剩余数据
        if(remainPktSize > 0) {
            rtpHeader->payload[0] = (naluType & 0x60) | 28;
            rtpHeader->payload[1] = naluType & 0x1F;
            rtpHeader->payload[1] |= 0x40; //end
           
            memcpy(rtpHeader->payload + 2, frame->mBuf + pos, remainPktSize);
            mRtpPacket.mSize = RTP_HEADER_SIZE + 2 + remainPktSize;
            sendRtpPacket(&mRtpPacket);
           
            mSeq++;
        }
    }
    
    mTimestamp += mClockRate / mFps;

}

