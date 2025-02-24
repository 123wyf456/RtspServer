#include "AACFileMediaSource.h"
#include "../Base/Log.h"
#include <string.h>

AACFileMediaSource* AACFileMediaSource::createNew(UsageEnvironment* env, const std::string& file) {
    return new AACFileMediaSource(env, file);
}

AACFileMediaSource::AACFileMediaSource(UsageEnvironment* env, const std::string& file) :
    MediaSource(env)
{
    mSourceName = file;
    mFile = fopen(file.c_str(), "rb");  // 打开AAC文件进行读取

    setFps(43); // 设置帧率

    for(int i = 0; i < DEFAULT_FRAME_NUM; ++i) {
        mEnv->threadPool()->addTask(mTask);
    }
}

AACFileMediaSource::~AACFileMediaSource() {
    fclose(mFile);
}

void AACFileMediaSource::handleTask() {
    std::lock_guard<std::mutex> lck(mMtx);

    if(mFrameInputQueue.empty()) {
        return;
    }
    MediaFrame* frame = mFrameInputQueue.front();

    frame->mSize = getFrameFromAACFile(frame->temp, FRAME_MAX_SIZE);
    if(frame->mSize < 0) {
        return;
    }
    frame->mBuf = frame->temp;

    mFrameInputQueue.pop();
    mFrameOutputQueue.push(frame);
}

bool AACFileMediaSource::parseAdtsHeader(uint8_t* in, struct AdtsHeader* res) {
    memset(res,0,sizeof(*res));

    if ((in[0] == 0xFF)&&((in[1] & 0xF0) == 0xF0))
    {
        res->id = ((unsigned int) in[1] & 0x08) >> 3;
        res->layer = ((unsigned int) in[1] & 0x06) >> 1;
        res->protectionAbsent = (unsigned int) in[1] & 0x01;
        res->profile = ((unsigned int) in[2] & 0xc0) >> 6;
        res->samplingFreqIndex = ((unsigned int) in[2] & 0x3c) >> 2;
        res->privateBit = ((unsigned int) in[2] & 0x02) >> 1;
        res->channelCfg = ((((unsigned int) in[2] & 0x01) << 2) | (((unsigned int) in[3] & 0xc0) >> 6));
        res->originalCopy = ((unsigned int) in[3] & 0x20) >> 5;
        res->home = ((unsigned int) in[3] & 0x10) >> 4;
        res->copyrightIdentificationBit = ((unsigned int) in[3] & 0x08) >> 3;
        res->copyrightIdentificationStart = (unsigned int) in[3] & 0x04 >> 2;
        res->aacFrameLength = (((((unsigned int) in[3]) & 0x03) << 11) |
                                (((unsigned int)in[4] & 0xFF) << 3) |
                                    ((unsigned int)in[5] & 0xE0) >> 5) ;
        res->adtsBufferFullness = (((unsigned int) in[5] & 0x1f) << 6 |
                                        ((unsigned int) in[6] & 0xfc) >> 2);
        res->numberOfRawDataBlockInFrame = ((unsigned int) in[6] & 0x03);
    
        return true;
    }
    else
    {
        LOGE("failed to parse adts header");
        return false;
    }
}

int AACFileMediaSource::getFrameFromAACFile(uint8_t* buf, int size) {
    if (!mFile) {
        return -1;
    }
    
    uint8_t tmpBuf[7];
    int ret;

    // 读取文件的前 7 字节，这些字节包括 ADTS 头部的内容
    ret = fread(tmpBuf,1,7,mFile);
    if(ret <= 0)
    {
        fseek(mFile,0,SEEK_SET);    // 如果读取失败，则回到文件开头重新读取
        ret = fread(tmpBuf,1,7,mFile);
        if(ret <= 0)
        {
            return -1;
        }
    }
    
    if(!parseAdtsHeader(tmpBuf, &mAdtsHeader))  // 解析错误
        return -1;
    
    if(mAdtsHeader.aacFrameLength > size)
        return -1;
    
    memcpy(buf, tmpBuf, 7);
    
    // 读取实际的 AAC 数据部分，并存入 buf 中。
    ret = fread(buf+7,1,mAdtsHeader.aacFrameLength-7,mFile);
    if(ret < 0)
    {
        LOGE("read error");
        return -1;
    }
    
    return mAdtsHeader.aacFrameLength;
}