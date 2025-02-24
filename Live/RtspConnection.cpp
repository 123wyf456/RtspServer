#include "RtspConnection.h"
#include "RtspServer.h"
#include "Rtp.h"
#include "MediaSessionManager.h"
#include "MediaSession.h"
#include "../Base/Version.h"
#include "../Base/Log.h"
#include <stdio.h>
#include <string.h>

static void getPeerIp(int fd, std::string& ip)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    getpeername(fd, (struct sockaddr*)&addr, &addrlen);
    ip = inet_ntoa(addr.sin_addr);
}

RtspConnection* RtspConnection::createNew(RtspServer* server, int clientFd) {
    return new RtspConnection(server, clientFd);
}

RtspConnection::RtspConnection(RtspServer* server, int clientFd) :
    TcpConnection(server->env(), clientFd),
    mRtspServer(server),
    mMethod(RtspConnection::Method::NONE),
    mTrackId(MediaSession::TrackId::TrackIdNone),
    mSessionId(rand()),
    mIsRtpOverTcp(false),
    mStreamPrefix("track")
{
    LOGI("RtspConnection() mClientFd=%d", mClientFd);

    for(int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        mRtpInstances[i] = NULL;
        mRtcpInstances[i] = NULL;
    }

    getPeerIp(clientFd, mPeerIp);
}

RtspConnection::~RtspConnection() {
    LOGI("~RtspConnection() mClientFd=%d", mClientFd);
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i)
    {
        if (mRtpInstances[i])
        {
            MediaSession* session = mRtspServer->mSessMgr->getMediaSession(mSuffix);

            if (session) {
                session->removeRtpInstance(mRtpInstances[i]);
            }
            else
                delete mRtpInstances[i];
        }

        if (mRtcpInstances[i])
        {
            delete mRtcpInstances[i];
        }
    }
}

void RtspConnection::handleReadBytes() {
    if(mIsRtpOverTcp) {
        if(mInputBuffer.peek()[0] == '$') {
            handleRtpOverTcp();
            return;
        }
    }

    // 解析请求
    if(!parseRequest()) {
        LOGE("parseRequest err");
        goto disConnect;
    }
    switch (mMethod)
    {
        case OPTIONS:
            if (!handleCmdOption()) // 处理OPTIONS请求
                goto disConnect;
            break;
        case DESCRIBE:
            if (!handleCmdDescribe())
                goto disConnect;
            break;
        case SETUP:
            if (!handleCmdSetup())
                goto disConnect;
            break;
        case PLAY:
            if (!handleCmdPlay())
                goto disConnect;
            break;
        case TEARDOWN:
            if (!handleCmdTeardown())
                goto disConnect;
            break;

        default:
            goto disConnect;
    }

    return;

disConnect:
    handleDisConnect();
}

// 解析客户端的 RTSP 请求，判断请求是否合法，确定请求的方法
bool RtspConnection::parseRequest() {
    // 解析第一行
    const char* crlf = mInputBuffer.findCRLF();
    if(crlf == NULL) {
        mInputBuffer.retrieveAll();
        return false;
    }

    bool ret = parseRequest1(mInputBuffer.peek(), crlf);
    if(ret == false) {
        mInputBuffer.retrieveAll();
        return false;
    }
    else {
        mInputBuffer.retrieveUntil(crlf + 2);   // 移除当前行数据，包括'\r\n'
    }

    // 解析后续行
    crlf = mInputBuffer.findLastCrlf();
    if(crlf == NULL) {
        mInputBuffer.retrieveAll();
        return false;
    }

    ret = parseRequest2(mInputBuffer.peek(), crlf);
    if(ret == false)
    {
        mInputBuffer.retrieveAll();
        return false;
    }
    else {
        mInputBuffer.retrieveUntil(crlf + 2);
        return true;
    }
}

// 辅助函数，解析请求的某个部分（如消息头），并处理具体的字段
bool RtspConnection::parseRequest1(const char* begin, const char* end){
    std::string message(begin, end);
    char method[64] = {0};
    char url[512] = {0};
    char version[64] = {0};

    if(sscanf(message.c_str(), "%s %s %s", method, url, version) != 3) {
        return false;
    }

    // 根据方法设置mMethod
    if(!strcmp(method, "OPTIONS")) {
        mMethod = OPTIONS;
    }
    else if (!strcmp(method, "DESCRIBE")) {
        mMethod = DESCRIBE;
    }
    else if (!strcmp(method, "SETUP")) {
        mMethod = SETUP;
    }
    else if (!strcmp(method, "PLAY")) {
        mMethod = PLAY;
    }
    else if (!strcmp(method, "TEARDOWN")) {
        mMethod = TEARDOWN;
    }
    else {
        mMethod = NONE;
        return false;
    }

    // 对url进一步解析
    if(strncmp(url, "rtsp://", 7) != 0) // 提取IP
        return false;

    uint16_t port = 0;
    char ip[64] = {0};
    char suffix[64] = {0};

    if(sscanf(url + 7, "%[^:]:%hu/%s", ip, suffix) == 2)
        port = 554;
    else if(sscanf(url + 7, "%[^/]/%s", ip, suffix) == 2)
        port = 554;
    else 
        return false;

    mUrl = url;
    mSuffix = suffix;

    return true;
}

// 辅助函数，继续解析请求的其他部分
bool RtspConnection::parseRequest2(const char* begin, const char* end) {
    std::string message(begin, end);

    if (!parseCSeq(message)) {
        return false;
    }
    if (mMethod == OPTIONS) {
        return true;
    }
    else if (mMethod == DESCRIBE) {
        return parseDescribe(message);
    }
    else if (mMethod == SETUP)
    {
        return parseSetup(message);
    }
    else if (mMethod == PLAY) {
        return parsePlay(message);
    }
    else if (mMethod == TEARDOWN) {
        return true;
    }
    else {
        return false;
    }
}

// 解析序列号，匹配请求和响应
bool RtspConnection::parseCSeq(std::string& message) {
    std::size_t pos = message.find("CSeq");
    if(pos != std::string::npos) {
        uint32_t cseq = 0;
        sscanf(message.c_str() + pos, "%*[^:]: %u", &cseq);
        mCSeq = cseq;
        return true;
    }

    return false;
}

// 解析描述请求，返回媒体会话的描述信息（SDP）
bool RtspConnection::parseDescribe(std::string& message) {
    if ((message.rfind("Accept") == std::string::npos)
        || (message.rfind("sdp") == std::string::npos))
    {
        return false;
    }

    return true;
}

// 解析Setup请求，处理设置RTP/RTCP数据流的细节
bool RtspConnection::parseSetup(std::string& message) {
    mTrackId = MediaSession::TrackIdNone;
    std::size_t pos;    // 保存查找字符串时的索引位置

    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; i++) {
        pos = mUrl.find(mStreamPrefix + std::to_string(i)); // 查找 mUrl 字符串中是否包含特定的流前缀和轨道编号
        if (pos != std::string::npos)
        {
            if (i == 0) {
                mTrackId = MediaSession::TrackId0;
            }
            else if(i == 1)
            {
                mTrackId = MediaSession::TrackId1;
            }
        }
    }
    if (mTrackId == MediaSession::TrackIdNone) {
        return false;
    }

    pos = message.find("Transport");
    if (pos != std::string::npos)
    {
        if ((pos = message.find("RTP/AVP/TCP")) != std::string::npos)
        {
            uint8_t rtpChannel, rtcpChannel;
            mIsRtpOverTcp = true;   // 使用RTP over TCP
    
            if (sscanf(message.c_str() + pos, "%*[^;];%*[^;];%*[^=]=%hhu-%hhu",
                       &rtpChannel, &rtcpChannel) != 2)
            {
                return false;
            }
    
            mRtpChannel = rtpChannel;
    
            return true;
        }
        else if ((pos = message.find("RTP/AVP")) != std::string::npos)
        {
            uint16_t rtpPort = 0, rtcpPort = 0;
            if (((message.find("unicast", pos)) != std::string::npos))
            {
                if (sscanf(message.c_str() + pos, "%*[^;];%*[^;];%*[^=]=%hu-%hu",
                           &rtpPort, &rtcpPort) != 2)
                {
                    return false;
                }
            }
            else if ((message.find("multicast", pos)) != std::string::npos)
            {
                return true;
            }
            else
                return false;
    
            mPeerRtpPort = rtpPort;
            mPeerRtcpPort = rtcpPort;
        }
        else
        {
            return false;
        }
    
        return true;
    }
    
    return false;

}

// 启动媒体播放的逻辑
bool RtspConnection::parsePlay(std::string& message) {
    std::size_t pos = message.find("Session");
    if (pos != std::string::npos)
    {
        uint32_t sessionId = 0;
        if (sscanf(message.c_str() + pos, "%*[^:]: %u", &sessionId) != 1)
            return false;
        return true;
    }

    return false;
}

// 返回服务器支持的 RTSP 方法
bool RtspConnection::handleCmdOption() {
    snprintf(mBuffer, sizeof(mBuffer), 
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %u\r\n"
            "Public: DESCRIBE, ANNOUNCE, SETUP, PLAY, RECORD, PAUSE, GET_PARAMETER, TEARDOWN\r\n"
            "Server: %s\r\n"
            "\r\n", mCSeq,PROJECT_VERSION);
    
    if(sendMessage(mBuffer, strlen(mBuffer)) < 0)
        return false;
    
    return true;
}

// 返回媒体流的描述信息
bool RtspConnection::handleCmdDescribe() {
    MediaSession* session = mRtspServer->mSessMgr->getMediaSession(mSuffix);

    if (!session) {
        LOGE("can't find session:%s", mSuffix.c_str());
        return false;
    }
    std::string sdp = session->generateSDPDescription();

    memset((void*)mBuffer, 0, sizeof(mBuffer));
    snprintf((char*)mBuffer, sizeof(mBuffer),
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %u\r\n"
             "Content-Length: %u\r\n"
             "Content-Type: application/sdp\r\n"
             "\r\n"
             "%s",
             mCSeq,
             (unsigned int)sdp.size(),
             sdp.c_str());

    if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
        return false;

    return true;
}

// 设置 RTP/RTCP 流的传输协议
bool RtspConnection::handleCmdSetup() {
    char sessionName[100];
    if (sscanf(mSuffix.c_str(), "%[^/]/", sessionName) != 1)
    {
        return false;
    }
    MediaSession* session = mRtspServer->mSessMgr->getMediaSession(sessionName); // 获取会话名称
    if (!session){
        LOGE("can't find session:%s",sessionName);
        return false;
    }
    
    if (mTrackId >= MEDIA_MAX_TRACK_NUM || mRtpInstances[mTrackId] || mRtcpInstances[mTrackId]) {
        return false;
    }
    
    if (session->isStartMulticast()) {  // 如果使用多播
        snprintf((char*)mBuffer, sizeof(mBuffer),
                 "RTSP/1.0 200 OK\r\n"
                 "CSeq: %d\r\n"
                 "Transport: RTP/AVP;multicast;"
                 "destination=%s;source=%s;port=%d-%d;ttl=255\r\n"
                 "Session: %08x\r\n"
                 "\r\n",
                 mCSeq,
                 session->getMulticastDestAddr().c_str(),
                 std::string ("0.0.0.0").c_str(),
                 session->getMulticastDestRtpPort(mTrackId),
                 session->getMulticastDestRtpPort(mTrackId) + 1,
                 mSessionId);
    }
    else {
    
        if (mIsRtpOverTcp)
        {
            //创建rtp over tcp
            createRtpOverTcp(mTrackId, mClientFd, mRtpChannel);
            mRtpInstances[mTrackId]->setSessionId(mSessionId);
    
            session->addRtpInstance(mTrackId, mRtpInstances[mTrackId]);
    
            snprintf((char*)mBuffer, sizeof(mBuffer),
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Server: %s\r\n"
                     "Transport: RTP/AVP/TCP;unicast;interleaved=%hhu-%hhu\r\n"
                     "Session: %08x\r\n"
                     "\r\n",
                     mCSeq,PROJECT_VERSION,
                     mRtpChannel,
                     mRtpChannel + 1,
                     mSessionId);
        }
        else 
        {
            //创建 rtp over udp
            if (createRtpRtcpOverUdp(mTrackId, mPeerIp, mPeerRtpPort, mPeerRtcpPort) != true)
            {
                LOGE("failed to createRtpRtcpOverUdp");
                return false;
            }
    
            mRtpInstances[mTrackId]->setSessionId(mSessionId);
            mRtcpInstances[mTrackId]->setSessionId(mSessionId);
    
           
            session->addRtpInstance(mTrackId, mRtpInstances[mTrackId]);
    
            snprintf((char*)mBuffer, sizeof(mBuffer),
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %u\r\n"
                     "Server: %s\r\n"
                     "Transport: RTP/AVP;unicast;client_port=%hu-%hu;server_port=%hu-%hu\r\n"
                     "Session: %08x\r\n"
                     "\r\n",
                     mCSeq,PROJECT_VERSION,
                     mPeerRtpPort,
                     mPeerRtcpPort,
                     mRtpInstances[mTrackId]->getLocalPort(),
                     mRtcpInstances[mTrackId]->getLocalPort(),
                     mSessionId);
        }
    
    }
    
    if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
        return false;
    
    return true;
}

// 开始播放媒体流
bool RtspConnection::handleCmdPlay() {
    snprintf((char*)mBuffer, sizeof(mBuffer),
    "RTSP/1.0 200 OK\r\n"
    "CSeq: %d\r\n"
    "Server: %s\r\n"
    "Range: npt=0.000-\r\n"
    "Session: %08x; timeout=60\r\n"
    "\r\n",
    mCSeq, PROJECT_VERSION,
    mSessionId);

    if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
    return false;

    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i)
    {
    if (mRtpInstances[i]) {
    mRtpInstances[i]->setAlive(true);
    }

    if (mRtcpInstances[i]) {
    mRtcpInstances[i]->setAlive(true);
    }

    }

    return true;
}

// 结束会话
bool RtspConnection::handleCmdTeardown() {
    snprintf((char*)mBuffer, sizeof(mBuffer),
    "RTSP/1.0 200 OK\r\n"
    "CSeq: %d\r\n"
    "Server: %s\r\n"
    "\r\n",
    mCSeq, PROJECT_VERSION);

    if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
    {
        return false;
    }

    return true;
}

// 发送信息到客户端
int RtspConnection::sendMessage(void* buf, int size) {
    LOGI("%s", buf);
    int ret;

    mOutputBuffer.append(buf, size);
    ret = mOutputBuffer.write(mClientFd);  // 将信息写入到客户端套接字
    mOutputBuffer.retrieveAll();

    return ret;
}

int RtspConnection::sendMessage() {
    int ret = mOutputBuffer.write(mClientFd);   // 将输出缓冲区的数据写到客户端fd
    mOutputBuffer.retrieveAll();    // 清空缓冲区内容
    return ret;
}

// 创建 RTP 和 RTCP 会话并通过 UDP 发送数据流
bool RtspConnection::createRtpRtcpOverUdp(MediaSession::TrackId trackId, std::string peerIp, 
                                        uint16_t peerRtpPort, uint16_t peerRtcpPort) 
{
    Sockets rtpSocket(0), rtcpSocket(0);
    int rtpSockfd, rtcpSockfd;
    int16_t rtpPort, rtcpPort;
    bool ret;

    if(mRtpInstances[trackId] || mRtcpInstances[trackId]) {
        return false;
    }

    int i;
    for (i = 0; i < 10; ++i){// 重试10次
        rtpSockfd = rtpSocket.CreateUdpSock();
        if (rtpSockfd < 0)
        {
            return false;
        }

        rtcpSockfd = rtcpSocket.CreateUdpSock();
        if (rtcpSockfd < 0)
        {
            ::close(rtpSockfd);
            return false;
        }

        uint16_t port = rand() & 0xfffe;
        if (port < 10000)
            port += 10000;

        // 设置rtp和rtcp端口号
        rtpPort = port;
        rtcpPort = port + 1;

        ret = rtpSocket.bind("0.0.0.0", rtpPort);
        if (ret != true)
        {
            ::close(rtpSockfd);
            ::close(rtcpSockfd);
            continue;
        }

        ret = rtcpSocket.bind("0.0.0.0", rtcpPort);
        if (ret != true)
        {
            ::close(rtpSockfd);
            ::close(rtcpSockfd);
            continue;
        }

        break;
    }

    if (i == 10)
        return false;

    mRtpInstances[trackId] = RtpInstance::createNewOverUdp(rtpSockfd, rtpPort,
                                                        peerIp, peerRtpPort);
    mRtcpInstances[trackId] = RtcpInstance::createNew(rtcpSockfd, rtcpPort,
                                                    peerIp, peerRtcpPort);

    return true;
}

// 创建 RTP 会话并通过 TCP 发送数据流
bool RtspConnection::createRtpOverTcp(MediaSession::TrackId trackId, int sockfd, uint8_t rtpChannel) {
    mRtpInstances[trackId] = RtpInstance::createNewOverTcp(sockfd, rtpChannel);

    return true;
}

// 处理通过 TCP 发送 RTP 数据流的逻辑
void RtspConnection::handleRtpOverTcp() {
    int num = 0;
    while (true)
    {
        num += 1;
        uint8_t* buf = (uint8_t*)mInputBuffer.peek();
        uint8_t rtpChannel = buf[1];
        int16_t rtpSize = (buf[2] << 8) | buf[3];
    
        int16_t bufSize = 4 + rtpSize;
    
        if (mInputBuffer.readableBytes() < bufSize) {
            // 缓存数据小于一个RTP数据包的长度
            return;
        }else {     // 根据RTP通道号处理不同类型的数据包
            if (0x00 == rtpChannel) {
                RtpHeader rtpHeader;
                parseRtpHeader(buf + 4, &rtpHeader);
                LOGI("num=%d,rtpSize=%d", num, rtpSize);
            }
            else if(0x01 == rtpChannel)
            {
                RtcpHeader rtcpHeader;
                parseRtcpHeader(buf + 4, &rtcpHeader);
    
                LOGI("num=%d,rtcpHeader.packetType=%d,rtpSize=%d", num, rtcpHeader.packetType, rtpSize);
            }
            else if (0x02 == rtpChannel) {
                RtpHeader rtpHeader;
                parseRtpHeader(buf + 4, &rtpHeader);
                LOGI("num=%d,rtpSize=%d", num, rtpSize);
            }
            else if (0x03 == rtpChannel)
            {
                RtcpHeader rtcpHeader;
                parseRtcpHeader(buf + 4, &rtcpHeader);
    
                LOGI("num=%d,rtcpHeader.packetType=%d,rtpSize=%d", num, rtcpHeader.packetType, rtpSize);
            }
    
            mInputBuffer.retrieve(bufSize);
        }
    }
}