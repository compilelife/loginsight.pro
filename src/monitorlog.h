#pragma once

#include "def.h"
#include "log.h"
#include "mem.h"
#include <list>
#include "platform.h"
#include "event2/event.h"
#include "event2/buffer.h"
#include "pingtask.h"

/**
 * @brief 每次达到最大缓存的时候，删除最旧的一个block
 */
class MonitorLog : public SourceLog{
private:
    struct MemBlock {
        Memory mem;
        Block block;
        //FIXME:极端情况下一个block里的行数会超过BLOCK_LINE_NUM
        string backend;//固定200k(不能太大，避免一次丢太多行），能放多少行就多少行，预计可以放 200 * 1024 / 200 = 1000行
        LogCharI writePos{0};
        bool isBackendFull() {return writePos >= backend.capacity();}
        string_view::iterator lastFind{nullptr};
    };
    
    size_t mMaxBlockCount{100};//约20M
    list<MemBlock*> mBlocks;
    ProcessInfo mProcess;

    event* mListenEvent;

    shared_ptr<PingTask> mPendingReadTask;
    string mLastBlockTail;//上一个block没有换行符的遗留文本

public:
    MonitorLog() {mAttrs = LOG_ATTR_DYNAMIC_RANGE | LOG_ATTR_SELF_CLOSE;}
    shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const override;
    Range range() const override;

public:
    virtual bool open(string_view cmdline, event_base* evbase);
    virtual void close() override;
    void setMaxBlockCount(size_t n);

public:
    void handleReadStdOut();

private:
    void startListenFd(event_base* evbase);
    MemBlock* peekBlock();
    bool canRemoveOldestBlock();
    bool handlePendingTask(bool restartPending);
    bool readStdOutInto(MemBlock* curBlock);
    void splitLinesForNewContent(MemBlock* curBlock);
};