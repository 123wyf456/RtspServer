#pragma once
#include "Sink.h"
#include "RtpInstance.h"

#include <string>
#include <list>

#define MEDIA_MAX_TRACK_NUM 2

class MediaSession {
public:
    enum TrackId {
        TrackIdNone = -1,
        TrackId0    = 0,
        TrackId1    = 1
    };

    static MediaSession* createNew(std::string sessionName);
    MediaSession(std::string sessionName);
    ~MediaSession();

public:
    std::string name() const { return mSessionName; }
    std::string generateSDPDescription();

    bool addSink(MediaSession::TrackId trackid, Sink* sink);
    bool addRtpInstance(MediaSession::TrackId trackid, RtpInstance* rtpInstance);
    bool removeRtpInstance(RtpInstance* rtpInstance);

    bool startMulticast();
    bool isStartMulticast();
    std::string getMulticastDestAddr() const { return mMulticastAddr; }
    uint16_t getMulticastDestRtpPort(TrackId trackId);

private:
    class Track {
    public:
        Sink* mSink;    // 轨道对应的数据生产者
        int mTrackId;   // 轨道ID
        bool mIsAlive;  // 轨道是否存活
        std::list<RtpInstance*> mRtpInstances;
    };

    Track* getTrack(MediaSession::TrackId trackId); // 获取轨道对象

    static void sendPacketCallback(void* arg1, void* arg2, void* packet, Sink::PacketType packetType);
    void handleSendRtpPacket(MediaSession::Track* track, RtpPacket* packet);

private:
    std::string mSessionName;
    std::string mSdp;
    Track mTracks[MEDIA_MAX_TRACK_NUM];

    bool mIsStartMulticast;
    std::string mMulticastAddr;
    RtpInstance* mMulticastRtpInstances[MEDIA_MAX_TRACK_NUM];
    RtcpInstance* mMulticastRtcpInstances[MEDIA_MAX_TRACK_NUM];

};