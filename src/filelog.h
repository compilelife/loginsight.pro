#pragma once

#include "def.h"
#include "log.h"
#include "mem.h"
#include "platform.h"
#include "calculation.h"

class FileLog: public IClosableLog {
private:
    friend class MultiFileLog;
    using BlockChain = vector<Block*>;
    Memory mBuf;
    MMapInfo mMapInfo;
    vector<Block*> mBlocks;
    LogLineI mCount;

public:
    shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const override;
    Range range() const override;

public:
    bool open(string_view path);
    shared_ptr<Promise> scheduleBuildBlocks();
    void close() override;

private:
    friend class FileLog_buildBlock_Test;
    friend class Memory_LockWhenView_Test;
    any buildBlock(bool* cancel, string_view buf);
    void collectBlocks(const vector<any>& rets);
};