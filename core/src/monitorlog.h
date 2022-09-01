#pragma once

#include "def.h"
#include "log.h"
#include "mem.h"
#include <list>
#include "platform.h"
#include "event2/event.h"
#include "event2/buffer.h"
#include "pingtask.h"
#include <mutex>
#include <thread>
#include <condition_variable>

#define MORNITOR_BLOCK_SIZE 204800

/**
 * @brief 每次达到最大缓存的时候，删除最旧的一个block
 */
class MonitorLog : public SourceLog{
private:
    struct MemBlock {
        Memory mem;
        Block block;
        string backend;//固定200k(不能太大，避免一次丢太多行），能放多少行就多少行，预计可以放 200 * 1024 / 200 = 1000行
        //新数据可以写入的开始位置
        LogCharI writePos{0};
        bool isBackendFull() {return writePos >= MORNITOR_BLOCK_SIZE || block.lines.size() >= BLOCK_LINE_NUM;}
        //上一次new line查找完成的位置+1，即这一次应该开始查找的位置
        //不等于writePos，因为可能上次的数据不是以\n结束的
        const char* lastFind{nullptr}; //string_view::iterator
    };
    
    size_t mMaxBlockCount{100};//约20M
    list<MemBlock*> mBlocks;
    ProcessInfo mProcess;

    string mLastBlockTail;//上一个block没有换行符的遗留文本

    mutex mReadProcessMutex;
    condition_variable mReadProcessCond;
    evbuffer* mProcessBuf;
    thread mReadProcessThd;

public:
    MonitorLog() {mAttrs = LOG_ATTR_DYNAMIC_RANGE | LOG_ATTR_MAY_DISCONNECT;}
    ~MonitorLog();
    shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const override;
    Range range() const override;

public:
    virtual bool open(string_view cmdline, event_base* evbase);
    virtual void close() override;
    void setMaxBlockCount(size_t n);

public:
    void handleReadStdOut();

private:
    void startListenFd();
    MemBlock* peekBlock();
    bool readStdOutInto(MemBlock* curBlock);
    void splitLinesForNewContent(MemBlock* curBlock);
};