#pragma once

#include "def.h"
#include "log.h"
#include "mem.h"
#include "platform.h"
#include "calculation.h"

class FileLog: public ILog {
private:
    using BlockChain = vector<Block*>;
    Memory mBuf;
    MMapInfo mMapInfo;
    vector<Block*> mBlocks;
    LogLineI mCount;

public:
    unique_ptr<LogView> view() const override;
    Range range() const override;

public:
    bool open(string_view path);
    Calculation& scheduleBuildBlocks();
    void close();

private:
    friend class FileLog_buildBlock_Test;
    any buildBlock(string_view buf);
    void collectBlocks(vector<any>& rets);
};