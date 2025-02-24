#include "MediaSource.h"
#include "../Base/Log.h"

MediaSource::MediaSource(UsageEnvironment* env) :
    mEnv(env),
    mFps(0)
{
    for(int i = 0; i < DEFAULT_FRAME_NUM; ++i) {
        mFrameInputQueue.push(&mFrames[i]);
    }

    mTask.setTaskCallback(taskCallback, this);
}

MediaSource::~MediaSource()
{
    LOGI("~MediaSource()");
}

MediaFrame* MediaSource::getFrameFromOutputQueue() {
    std::lock_guard<std::mutex> lck(mMtx);
    if(mFrameOutputQueue.empty()) {
    return NULL;
    }
    MediaFrame* frame = mFrameOutputQueue.front();
    mFrameOutputQueue.pop();
    
    return frame;
}

void MediaSource::putFrameToInputQueue(MediaFrame* frame) {
    std::lock_guard<std::mutex> lck(mMtx);
    mFrameInputQueue.push(frame);

    mEnv->threadPool()->addTask(mTask); // 向线程池添加任务的目的是异步处理帧数据，提高系统的并发能力，避免阻塞主线程
}

void MediaSource::taskCallback(void* arg) {
    MediaSource* source = (MediaSource*)arg;
    source->handleTask();   // 根据媒体类型（aac、h264）调用回调函数(在子类中实现具体功能)
}