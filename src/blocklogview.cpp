#include "blocklogview.h"
#include "mem.h"
#include "log.h"
#include <algorithm>

BlockLogView::BlockLogView(const vector<Block*>& blocks, const Memory* memory, BlockLineI firstBlockLineIndex, BlockLineI finalLineInBlock) 
    :mFirstLineInBlock(firstBlockLineIndex),
    mLineIndexInBlock(firstBlockLineIndex), 
    mFinalLineInBlock(finalLineInBlock),
    mBlockIndex(0)
{
    size_t count = 0;
    for_each_n(blocks.begin(), blocks.size(), [&count](Block* b){count+=b->lines.size();});
    mCount = count - firstBlockLineIndex;

    if (mFinalLineInBlock == BLOCK_LINE_NUM && !blocks.empty()) {
        mFinalLineInBlock = LastIndex(LastItem(blocks)->lines);
    }

    auto mem = const_cast<Memory*>(memory);
    for (auto &&block : blocks)
    {
        mBlocks.push_back({
            block,
            mem
        });
    }
}

BlockLogView::BlockLogView(vector<BlockRef>&& blocks)
    :mBlocks(blocks), 
    mFirstLineInBlock(0),
    mLineIndexInBlock(0),
    mBlockIndex(0) {
    
    size_t count = 0;
    for_each_n(blocks.begin(), blocks.size(), [&count](BlockRef& b){count+=b.block->lines.size();});
    mCount = count;

    mFinalLineInBlock = LastIndex(LastItem(mBlocks).block->lines);
}

LogLineI BlockLogView::size() const {
    return mCount;
}

LineRef BlockLogView::current() const {
    auto&& curBlockRef = mBlocks[mBlockIndex];
    return {
        curBlockRef,
        &(curBlockRef.block->lines[mLineIndexInBlock]),
        mLineIndexInBlock
    };
}

void BlockLogView::next() {
    auto& curBlockRef = mBlocks[mBlockIndex];
    ++mLineIndexInBlock;
    
    if (mLineIndexInBlock >= curBlockRef.block->lines.size()) {
        mBlockIndex++;
        mLineIndexInBlock = 0;
    }
}

bool BlockLogView::end() {
    auto lastBlockIndex = LastIndex(mBlocks);
    return mBlockIndex > lastBlockIndex || (mBlockIndex == lastBlockIndex && mLineIndexInBlock > mFinalLineInBlock);
}

shared_ptr<LogView> BlockLogView::subview(LogLineI from, LogLineI n) const {
    auto [fromBlock, fromBlockLine] = locateLine(from);
    auto [toBlock, toBlockLine] = locateLine(from + n - 1);

    auto ret = new BlockLogView;

    auto fromIt = mBlocks.begin() + fromBlock;
    std::copy_n(fromIt, toBlock - fromBlock + 1, back_inserter(ret->mBlocks));
    ret->mCount = n;
    ret->mFinalLineInBlock = toBlockLine;
    ret->mLineIndexInBlock = fromBlockLine;
    ret->mBlockIndex = 0;

    return shared_ptr<LogView>(ret);
}

pair<size_t, BlockLineI> BlockLogView::locateLine(LogLineI line) const {
    LogLineI cur = 0;
    auto target = line + 1;

    size_t blockIndex = 0;
    cur += mBlocks[0].block->lines.size() - mFirstLineInBlock;
    auto lastBlock =  LastIndex(mBlocks);

    while (cur < target && blockIndex < lastBlock)
    {
        cur += mBlocks[++blockIndex].block->lines.size();
    }
    
    // 看看超出了多少，需要回吐
    auto lineIndex = LastIndex(mBlocks[blockIndex].block->lines) - (cur - target);
    
    //修正最后一个块索引可能超出的情况
    if (blockIndex == LastIndex(mBlocks) && lineIndex > mFinalLineInBlock) {
        lineIndex = mFinalLineInBlock;
    }

    return {blockIndex, lineIndex};
}