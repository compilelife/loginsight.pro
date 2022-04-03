#pragma once

#include "log.h"
#include <functional>
#include "calculation.h"
#include "promisex.h"
#include "sublogview.h"

struct FilterBlock {
    BlockRef blockRef;
    vector<BlockLineI> lines;
};

class SubLog: public ILog {
private:
    friend class SubLogView;
    vector<FilterBlock> mBlocks;
    LogLineI mCount;
private:
    SubLog(vector<FilterBlock>&& blocks, LogLineI count);
public:
    shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const override;
    Range range() const override;

public:
    static unique_ptr<Promise> createSubLog(shared_ptr<LogView> iter, FilterFunction predict);
};