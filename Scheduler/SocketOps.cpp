#include "SocketOps.h"
#include "../Base/Log.h"

#include <unistd.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

Sockets::Sockets(int sockfd)
    : sockfd_(sockfd)
{}

Sockets::~Sockets()
{
    close(sockfd_);
}

int Sockets::CreateTcpSock() {
    sockfd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    return sockfd_;
}

int Sockets::CreateUdpSock() {
    sockfd_ = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    return sockfd_;
}

bool Sockets::bind(std::string ip, uint16_t port) {
    struct sockaddr_in addr;
    // 服务器地址信息
    addr.sin_family = AF_INET;  // Ipv4
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);

    if(::bind(sockfd_, (struct sockaddr*)&addr, sizeof(sockaddr_in)) < 0) {
        LOGE("bind error, fd=%d, ip=%s, port=%d", sockfd_, ip.c_str(), port);
        return false;
    }

    return true;
}

bool Sockets::listen(int backlog) {
    if(::listen(sockfd_, backlog) < 0) {
        LOGE("::listen error,fd=%d,backlog=%d", sockfd_, backlog);
        return false;
    }
    return true;
}

int Sockets::accept() {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    int connfd = ::accept(sockfd_, (struct sockaddr*)&addr, &addrlen);
    setNonBlockAndCloseOnExec(connfd);  // 设置非阻塞模式 并在进程关闭时自动释放
    ignoreSigPipeOnSocket(connfd);      // 忽略 SIGPIPE 信号，避免管道破裂（Linux）

    return connfd;
}

void Sockets::setNonBlockAndCloseOnExec(int sockfd) {
    // non-block
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);

    // close-on-exec
    flags = ::fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    ret = ::fcntl(sockfd, F_SETFD, flags);
}

void Sockets::ignoreSigPipeOnSocket(int socketfd) {
    int option = 1;
    setsockopt(socketfd, SOL_SOCKET, MSG_NOSIGNAL, &option, sizeof(option));
}

void Sockets::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

void Sockets::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Sockets::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Sockets::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}

void Sockets::close(int sockfd)
{
#ifndef WIN32
    int ret = ::close(sockfd);
#else
    int ret = ::closesocket(sockfd);
#endif

}