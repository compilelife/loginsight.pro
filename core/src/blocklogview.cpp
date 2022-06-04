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

BlockLogView::~BlockLogView() {
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
    if (mReverse) {
        --mLineIndexInBlock;
        if (mLineIndexInBlock < 0) {
            mBlockIndex--;
            if (mBlockIndex >= 0)
                mLineIndexInBlock = LastIndex(mBlocks[mBlockIndex].block->lines);
        }
    } else {
        ++mLineIndexInBlock;
    
        if (mLineIndexInBlock >= curBlockRef.block->lines.size()) {
            mBlockIndex++;
            mLineIndexInBlock = 0;
        }
    }
}

bool BlockLogView::end() {
    if (mReverse) {
        return mBlockIndex < 0 || (mBlockIndex == 0 && mLineIndexInBlock < mFirstLineInBlock);
    } else {
        auto lastBlockIndex = LastIndex(mBlocks);
        return mBlockIndex > lastBlockIndex || (mBlockIndex == lastBlockIndex && mLineIndexInBlock > mFinalLineInBlock);
    }

    return false;    
}

void BlockLogView::reverse() {
    mReverse = true;
    mBlockIndex = LastIndex(mBlocks);
    mLineIndexInBlock = mFinalLineInBlock;
}

shared_ptr<LogView> BlockLogView::subview(LogLineI from, LogLineI n) const {
    auto [fromBlock, fromBlockLine] = locateLine(from);
    auto [toBlock, toBlockLine] = locateLine(from + n - 1);

    auto ret = new BlockLogView;
    ret->mCount = n;
    ret->mReverse = mReverse;

    if (mReverse) {
        //此时from>to，所以下面的计算需要反过来，以保证ret中的blocks是正序的
        auto fromIt = mBlocks.begin() + toBlock;
        std::copy_n(fromIt, fromBlock - toBlock + 1, back_inserter(ret->mBlocks));
        ret->mFinalLineInBlock = fromBlockLine;
        ret->mFirstLineInBlock = toBlockLine;
        ret->reverse();
    } else {
        auto fromIt = mBlocks.begin() + fromBlock;
        std::copy_n(fromIt, toBlock - fromBlock + 1, back_inserter(ret->mBlocks));
        ret->mFinalLineInBlock = toBlockLine;
        ret->mBlockIndex = 0;
        ret->mLineIndexInBlock = fromBlockLine;
        ret->mFirstLineInBlock = fromBlockLine;
    }

    return shared_ptr<LogView>(ret);
}

pair<size_t, BlockLineI> BlockLogView::locateLine(LogLineI line) const {
    LogLineI cur = 0;
    auto target = line + 1;

    if (mReverse) {
        size_t blockIndex = LastIndex(mBlocks);
        cur += (mFinalLineInBlock+1);

        while (cur < target && blockIndex > 0)
        {
            cur += mBlocks[--blockIndex].block->lines.size();
        }
        
        // 看看超出了多少，需要回吐
        auto lineIndex = cur - target;

        //修正最后一个块索引可能超出的情况
        if (blockIndex == 0 && lineIndex < mFirstLineInBlock) {
            lineIndex = mFirstLineInBlock;
        }

        return {blockIndex, lineIndex};
    } else {
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

    return {0,0};
}