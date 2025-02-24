#pragma once
#include <string>
#include <map>
#include "MediaSession.h"
#include "TcpConnection.h"

class RtspServer;

class RtspConnection : public TcpConnection {
public:
    enum Method
    {
        OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN,
        NONE,
    };

    static RtspConnection* createNew(RtspServer* server, int clientFd);

    RtspConnection(RtspServer* server, int clientFd);
    virtual ~RtspConnection();

protected:
    virtual void handleReadBytes();

private:
    bool parseRequest();    // 解析客户端的 RTSP 请求，判断请求是否合法，确定请求的方法
    bool parseRequest1(const char* begin, const char* end); // 辅助函数，解析请求的某个部分（如消息头），并处理具体的字段
    bool parseRequest2(const char* begin, const char* end); // 辅助函数，继续解析请求的其他部分

    bool parseCSeq(std::string& message);       // 解析序列号，匹配请求和响应
    bool parseDescribe(std::string& message);   // 解析描述请求，返回媒体会话的描述信息（SDP）
    bool parseSetup(std::string& message);      // 解析Setup请求，处理设置RTP/RTCP数据流的细节
    bool parsePlay(std::string& message);       // 启动媒体播放的逻辑

    bool handleCmdOption();     // 返回服务器支持的 RTSP 方法
    bool handleCmdDescribe();   // 返回媒体流的描述信息
    bool handleCmdSetup();      // 设置 RTP/RTCP 流的传输协议
    bool handleCmdPlay();       // 开始播放媒体流
    bool handleCmdTeardown();   // 结束会话

    int sendMessage(void* buf, int size);   // 发送信息到客户端
    int sendMessage();

    bool createRtpRtcpOverUdp(MediaSession::TrackId trackId, std::string peerIp,    // 创建 RTP 和 RTCP 会话并通过 UDP 发送数据流
        uint16_t peerRtpPort, uint16_t peerRtcpPort);
    bool createRtpOverTcp(MediaSession::TrackId trackId, int sockfd, uint8_t rtpChannel);   // 创建 RTP 会话并通过 TCP 发送数据流

    void handleRtpOverTcp();    // 处理通过 TCP 发送 RTP 数据流的逻辑

private:
    RtspServer* mRtspServer;
    std::string mPeerIp;
    Method mMethod;
    std::string mUrl;
    std::string mSuffix;
    uint32_t mCSeq;
    std::string mStreamPrefix;

    uint16_t mPeerRtpPort;
    uint16_t mPeerRtcpPort;

    MediaSession::TrackId mTrackId;
    RtpInstance* mRtpInstances[MEDIA_MAX_TRACK_NUM];
    RtcpInstance* mRtcpInstances[MEDIA_MAX_TRACK_NUM];

    int mSessionId;
    bool mIsRtpOverTcp;
    uint8_t mRtpChannel;

};