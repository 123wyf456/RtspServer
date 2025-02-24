#include "Buffer.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int Buffer::initialSize = 1024;
const char* Buffer::kCRLF = "\r\n";

Buffer::Buffer() :
    mBufferSize(initialSize),
    mReadIndex(0),
    mWriteIndex(0)
{
    mBuffer = (char*)malloc(mBufferSize);
}

Buffer::~Buffer()
{
    free(mBuffer);
}

// 从指定的文件描述符 fd 中读取数据，并将数据存入缓冲区
int Buffer::read(int fd) {
    char extrabuf[65536];   // 定义一个 64KB 的临时缓冲区，防止 mBuffer 空间不足导致 recv 失败
    const int writable = writableBytes();   // 获取当前缓冲区可写字节数
    const int n = ::recv(fd, extrabuf, sizeof(extrabuf), 0);    // 从fd读取数据，存入extrabuf
    if (n <= 0) {
        return -1;
    }
    else if (n <= writable)
    {
        std::copy(extrabuf, extrabuf + n, beginWrite()); // 拷贝数据到mBuffer
        mWriteIndex += n;
    }
    else
    {
        std::copy(extrabuf, extrabuf + writable, beginWrite()); // 拷贝数据
        mWriteIndex += writable;
        append(extrabuf+ writable, n - writable);
    }
    return n;
}

// 将 buffer 的可读数据写入 fd
int Buffer::write(int fd) {
    return ::write(fd, peek(), readableBytes());
}