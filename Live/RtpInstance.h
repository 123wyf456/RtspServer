#ifndef BXC_RTSPSERVER_RTPINSTANNCE_H
#define BXC_RTSPSERVER_RTPINSTANNCE_H
#include <string>
#include <stdint.h>
#ifndef WIN32
#include <unistd.h>
#endif // !WIN32
#include "InetAddress.h"
#include "../Scheduler/SocketOps.h"
#include "Rtp.h"


class RtpInstance
{
public:
    enum RtpType
    {
        RTP_OVER_UDP,
        RTP_OVER_TCP
    };

    static RtpInstance* createNewOverUdp(int localSockfd, uint16_t localPort,
                                         std::string destIp, uint16_t destPort)
    {
        return new RtpInstance(localSockfd, localPort, destIp, destPort);
    }

    static RtpInstance* createNewOverTcp(int sockfd, uint8_t rtpChannel)
    {
        return new RtpInstance(sockfd, rtpChannel);
    }

    ~RtpInstance()
    {
        ::close(mSockfd);
    }
    uint16_t getLocalPort() const { return mLocalPort; }    // 获取本地端口
    uint16_t getPeerPort() { return mDestAddr.getPort(); }  // 获取对端端口

    // 发送RTP数据包
    int send(RtpPacket* rtpPacket)
    {
        switch (mRtpType)
        {
            case RtpInstance::RTP_OVER_UDP: {
                return sendOverUdp(rtpPacket->mBuf4, rtpPacket->mSize);
                break;
            }
            case RtpInstance::RTP_OVER_TCP: {
                rtpPacket->mBuf[0] = '$';
                rtpPacket->mBuf[1] = (uint8_t)mRtpChannel;  // 通道号

                // 数据包大小（高8位和低8位）
                rtpPacket->mBuf[2] = (uint8_t)(((rtpPacket->mSize) & 0xFF00) >> 8);
                rtpPacket->mBuf[3] = (uint8_t)((rtpPacket->mSize) & 0xFF);
                return sendOverTcp(rtpPacket->mBuf, 4 + rtpPacket->mSize);
                break;
            }

            default: {
                return -1;
                break;
            }
        }
    }

    bool alive() const { return mIsAlive; }
    int setAlive(bool alive) { mIsAlive = alive; return 0; };
    void setSessionId(uint16_t sessionId) { mSessionId = sessionId; }
    uint16_t sessionId() const { return mSessionId; }

private:
    int sendOverUdp(void * buf, int size)
    {
        return ::sendto(mSockfd, buf, size, 0, mDestAddr.getAddr(), sizeof(struct sockaddr));
    }

    int sendOverTcp(void * buf, int size)
    {
        return write(mSockfd, buf, size);
    }

public:
    RtpInstance(int localSockfd, uint16_t localPort, const std::string& destIp, uint16_t destPort) :
        mRtpType(RTP_OVER_UDP), 
        mSockfd(localSockfd), mLocalPort(localPort),mDestAddr(destIp, destPort), 
        mIsAlive(false), 
        mSessionId(0),
        mRtpChannel(0) {
    }

    RtpInstance(int sockfd, uint8_t rtpChannel) : 
        mRtpType(RTP_OVER_TCP), 
        mSockfd(sockfd),mLocalPort(0),
        mIsAlive(false), 
        mSessionId(0),
        mRtpChannel(rtpChannel){
    }


private:
    RtpType mRtpType;
    int mSockfd;
    uint16_t mLocalPort; //for udp
    Ipv4Address mDestAddr; //for udp
    bool mIsAlive;
    uint16_t mSessionId;
    uint8_t mRtpChannel;
};

// RTCP类，管理RTP传输
class RtcpInstance
{
public:
    static RtcpInstance* createNew(int localSockfd, uint16_t localPort,
                                   std::string destIp, uint16_t destPort)
    {
        return new RtcpInstance(localSockfd, localPort, destIp, destPort);
//        return New<RtcpInstance>::allocate(localSockfd, localPort, destIp, destPort);
    }

    int send(void* buf, int size)
    {
        return ::sendto(mLocalSockfd, buf, size, 0, mDestAddr.getAddr(), sizeof(struct sockaddr));
    }

    int recv(void* buf, int size, Ipv4Address* addr)
    {
        //TODO
        return 0;
    }

    uint16_t getLocalPort() const { return mLocalPort; }

    int alive() const { return mIsAlive; }
    int setAlive(bool alive) { mIsAlive = alive; return 0; };
    void setSessionId(uint16_t sessionId) { mSessionId = sessionId; }
    uint16_t sessionId() const { return mSessionId; }

public:
    RtcpInstance(int localSockfd, uint16_t localPort,
                 std::string destIp, uint16_t destPort) :
            mLocalSockfd(localSockfd), mLocalPort(localPort), mDestAddr(destIp, destPort),
            mIsAlive(false), mSessionId(0)
    {
    }
    ~RtcpInstance()
    {
        ::close(mLocalSockfd);
    }
private:
    int mLocalSockfd;
    uint16_t mLocalPort;
    Ipv4Address mDestAddr;
    bool mIsAlive;
    uint16_t mSessionId;

    // Sockets mSockets;
};

#endif //BXC_RTSPSERVER_RTPINSTANNCE_H