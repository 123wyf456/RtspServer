#pragma once

class Sockets {
public:
    Sockets(int sockfd);
    ~Sockets();

    int CreateTcpSock();
    int CreateUdpSock();
    int fd() const { return sockfd_; }

    bool bind(std::string ip, uint16_t port);
    bool listen(int backlog);
    int accept();

    void setNonBlockAndCloseOnExec(int sockfd);     // 设置非阻塞模式，并在进程关闭时自动释放
    void ignoreSigPipeOnSocket(int socketfd);       // 忽略SIGPIPE信号，避免管道破裂

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

    void close(int fd);
    bool connect(const std::string& ip, uint16_t port, int timeout);

    std::string getPeerIp(int sockfd);
    int16_t getPeerPort(int sockfd);
    std::string getLocalIp();

private:
    int sockfd_;
};