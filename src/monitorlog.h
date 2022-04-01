#pragma once

#include "def.h"
#include "log.h"
#include "mem.h"
#include <list>
#include "platform.h"
#include "event2/event.h"
#include "event2/buffer.h"

/**
 * @brief 每次达到最大缓存的时候，删除最旧的一个block
 */
class MonitorLog : public ILog {
private:
    struct MemBlock {
        Memory mem;
        Block block;
        string backend;//固定5M，能放多少行就多少行，预计可以放 5 * 1024 * 1024 / 200 = 26k行
    };
    
    size_t mMaxBlockCount{10};//约50M
    list<unique_ptr<MemBlock>> mBlocks;
    ProcessInfo mProcess;

    event* mListenEvent;

public:
    shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const override;
    Range range() const override;

public:
    bool open(string_view cmdline, event_base* evbase);
    void close();
    void setMaxBlockCount(size_t n);

public:
    void handleReadStdOut();

private:
    void startListenFd(event_base* evbase);
    MemBlock& peekBlock();
};