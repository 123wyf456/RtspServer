#pragma once
#include <stdlib.h>
#include <algorithm>
#include <stdint.h>
#include <assert.h>

class Buffer {
public:
    static const int initialSize;

    explicit Buffer();
    ~Buffer();

    int readableBytes() const {
        return mWriteIndex - mReadIndex;
    }

    int writableBytes() const {
        return mBufferSize - mWriteIndex;
    }

    // 返回可以前置的字节数（即当前已经读过的数据）
    int prependableBytes() const {
        return mReadIndex;
    }

    // 返回一个指向当前可读取数据的指针
    char* peek() {
        return begin() + mReadIndex;
    }

    const char* peek() const {
        return begin() + mReadIndex;
    }

    // 查找缓冲区中的CRLF
    const char* findCRLF() const {
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    const char* findCRLF(const char* start) const {
		assert(peek() <= start);    // 确保peek() <= start
		assert(start <= beginWrite());
		const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF + 2);
		return crlf == beginWrite() ? NULL : crlf;
    }

    // 查找缓冲区最后一次出现CRLF的位置
    const char* findLastCrlf() const
	{
		const char* crlf = std::find_end(peek(), beginWrite(), kCRLF, kCRLF + 2);
		return crlf == beginWrite() ? NULL : crlf;
	}

    // 自定义函数（本次读取的数据不全，已读取的数据归零，重新从头读取）
    void retrieveReadZero() {
		mReadIndex = 0;
	}

    // 从缓冲区读取len字节的数据
    void retrieve(int len) {
        assert(len <= readableBytes());
        if(len < readableBytes()) {
            mReadIndex += len;
        }
        else {
            retrieveAll();
        }
    }

    // 读取数据直到指定位置
	void retrieveUntil(const char* end)
	{
		assert(peek() <= end);
		assert(end <= beginWrite());
		retrieve(end - peek());
	}

    // 恢复索引
    void retrieveAll()
	{
		mReadIndex = 0;
		mWriteIndex = 0;
	}

    // 返回一个指向当前可写数据的指针
    char* beginWrite()
	{
		return begin() + mWriteIndex;
	}

	const char* beginWrite() const
	{
		return begin() + mWriteIndex;
	}

    // 撤回已经写入的数据
	void unwrite(int len)
	{
		assert(len <= readableBytes());
		mWriteIndex -= len;
	}

    // 确保有足够的空间
    void ensureWritableBytes(int len) {
        if(writableBytes() < len)
            makeSpace(len); // 扩容
        assert(writableBytes() >= len);
    }

    // 扩容
    void makeSpace(int len) {
        // 剩余空间不足
        if(writableBytes() + prependableBytes() < len) {
            mBufferSize = mWriteIndex + len;
            mBuffer = (char*)realloc(mBuffer, mBufferSize);
        }
        // 剩余空间足够
        else {
            int readable = readableBytes();
            std::copy(begin() + mReadIndex, begin() + mWriteIndex, begin());
            mReadIndex = 0;
            mWriteIndex = mReadIndex + readable;
            assert(readable == readableBytes());
        }
    }

    // 将 len 长度的数据 data 添加到缓冲区中
	void append(const char* data, int len)
	{
		ensureWritableBytes(len); // 调整扩大的空间
		std::copy(data, data + len, beginWrite()); // 拷贝数据

		assert(len <= writableBytes());
		mWriteIndex += len;// 重新调节写位置
	}

	void append(const void* data, int len)
	{
		append((const char*)(data), len);
	}

	int read(int fd);
	int write(int fd);

private:
    char* begin() {
        return mBuffer;
    }

    const char* begin() const {
        return mBuffer;
    }

private:
    char* mBuffer;
    int mBufferSize;
    int mReadIndex;
    int mWriteIndex;

    static const char* kCRLF;

};