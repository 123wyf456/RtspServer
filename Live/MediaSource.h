#pragma once
#include "../Scheduler/UsageEnvironment.h"
#include "../Scheduler/ThreadPool.h"

#include <queue>
#include <mutex>
#include <stdint.h>

#define FRAME_MAX_SIZE (1024*200)
#define DEFAULT_FRAME_NUM 4

class MediaFrame {
public:
    MediaFrame() :
        temp(new uint8_t[FRAME_MAX_SIZE]),
        mBuf(nullptr),
        mSize(0)
    {}
    ~MediaFrame() {
        delete []temp;
    }

    uint8_t* temp;  // 暂存容器
    uint8_t* mBuf;  // 存放帧内容
    int mSize;
};

class MediaSource {
public:
    explicit MediaSource(UsageEnvironment* env);
    virtual ~MediaSource();

    MediaFrame* getFrameFromOutputQueue();  // 从输出队列获取帧
    void putFrameToInputQueue(MediaFrame* frame);
    int getFps() const { return mFps; }
    std::string getSourceName() { return mSourceName; }

private:
    static void taskCallback(void* arg);

protected:
    virtual void handleTask() = 0;
    void setFps(int fps) { mFps = fps; }

protected:
    UsageEnvironment* mEnv; // 运行环境
    MediaFrame mFrames[DEFAULT_FRAME_NUM];  // 预分配帧缓存
    std::queue<MediaFrame*> mFrameInputQueue;   // 输入帧队列
    std::queue<MediaFrame*> mFrameOutputQueue;  // 输出帧队列

    std::mutex mMtx;
    ThreadPool::Task mTask;     // 线程池任务
    int mFps;   // 帧率
    std::string mSourceName;    // 数据源名称

};