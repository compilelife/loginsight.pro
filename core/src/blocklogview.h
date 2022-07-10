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
    
    int mLineIndexInBlock;
    int mBlockIndex;

    /**
     * reverse的实现方式如下：
     * 上述blocks及相关的字段仍然是正序的，
     * 对于next，从final -> first 递减
     * 对于end，判断是否越过frist边界
     * 对于subview的调用参数，第0行，是正序的最后一行，所以locateLine也将第0行解析为正序最后一行的位置；
     * 然后subview中构造一个reverse的view，这个view的blocks及相关的字段也是正序的（及locateLine(to)的位置信息指给first）
     */
    bool mReverse{false};
private:
    BlockLogView(){}
    void lockMemory();
    void unlockMemory();
public:
    ~BlockLogView();
    BlockLogView(const vector<Block*>& blocks, 
            const Memory* memory, 
            BlockLineI firstLineInBlock = 0,
            BlockLineI finalLineInBlock = BLOCK_LINE_NUM);

    BlockLogView(vector<BlockRef>&& blocks);

public:
    LogLineI size() const override;
    LineRef current() const override;
    void next() override;
    bool end() override;
    shared_ptr<LogView> subview(LogLineI from, LogLineI n) const override;
    void reverse() override;

private:
    pair<size_t, BlockLineI> locateLine(LogLineI line) const;
};