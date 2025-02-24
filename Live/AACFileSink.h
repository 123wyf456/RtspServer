#pragma once
#include "Sink.h"

class AACFileSink : public Sink {
public:
    static AACFileSink* createNew(UsageEnvironment* env, MediaSource* mediaSource);

    AACFileSink(UsageEnvironment* env, MediaSource* mediaSource, int payloadType);
    ~AACFileSink();

    virtual std::string getMediaDescription(uint16_t port);
    virtual std::string getAttribute();
    virtual void sendFrame(MediaFrame* frame);

private:
    RtpPacket mRtpPacket;
    uint32_t mSampleRate;
    uint32_t mChannels;
    int mFps;

};