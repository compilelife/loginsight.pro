#pragma once

#include "def.h"
#include <vector>
#include <utility>
#include "logview.h"

struct Block;
class Memory;

class BlockLogView : public LogView {
private:
    vector<BlockRef> mBlocks;

    LogLineI mCount;
    BlockLineI mFirstLineInBlock;
    BlockLineI mFinalLineInBlock;
    BlockLineI mLineIndexInBlock;
    size_t mBlockIndex;
private:
    BlockLogView(){}
public:
    BlockLogView(const vector<Block*>& blocks, 
            const Memory* memory, 
            BlockLineI firstLineInBlock = 0,
            BlockLineI finalLineInBlock = BLOCK_LINE_NUM);

public:
    LogLineI size() const override;
    LineRef current() const override;
    void next() override;
    unique_ptr<LogView> subview(LogLineI from, LogLineI n) const override;

private:
    pair<size_t, BlockLineI> locateLine(LogLineI line) const;
};