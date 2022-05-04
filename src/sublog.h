#pragma once

#include "log.h"
#include <functional>
#include "calculation.h"
#include "promisex.h"
#include "sublogview.h"

struct FilterBlock {
    BlockRef blockRef;
    vector<BlockLineI> lines;//每个元素的值表示其在源Block里的索引
    LogLineI mapLineIndexToSource(BlockLineI i) const;
    Range sourceLineRange() const;
};

class SubLog: public ILog {
private:
    friend class SubLogView;
    vector<FilterBlock> mBlocks;
    LogLineI mCount;
public:
    SubLog(vector<FilterBlock>&& blocks, LogLineI count);
public:
    shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const override;
    Range range() const override;
    LogLineI mapToSource(LogLineI index) const override ;
    LogLineI fromSource(LogLineI index) const override ;

public:
    //value as shared_ptr<SubLog>
    static shared_ptr<Promise> createSubLog(shared_ptr<LogView> iter, FilterFunction predict);
};