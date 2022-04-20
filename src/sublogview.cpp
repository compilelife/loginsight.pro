#include "sublogview.h"
#include "sublog.h"
#include "mem.h"
#include "stdout.h"

SubLogView::SubLogView(const SubLog* log)
    :mLog(const_cast<SubLog*>(log)) {
    mFrom = {0,0};
    mTo = {LastIndex(log->mBlocks), LastIndex(LastItem(log->mBlocks).lines)};
    mCur = mFrom;
    mCount = log->mCount;
    lockMemory();
}

SubLogView::SubLogView(SubLog* log, SubLogPos from, SubLogPos to, LogLineI n) 
    :mLog(log), mFrom(from), mTo(to), mCur(mFrom), mCount(n) {
    lockMemory();
}

static Range bytesRange(FilterBlock& b, size_t from, size_t to) {
    auto block = b.blockRef.block;
    auto& fl =  block->lines[b.lines[from]];
    if (to >= b.lines.size()) {
        to = LastIndex(b.lines);
    }
    auto &tl = block->lines[b.lines[to]];

    return {
        block->offset + fl.offset,
        block->offset + tl.offset + tl.length - 1
    };
}

void SubLogView::lockMemory() {
    auto lock = [](FilterBlock& b, Range& range){
        if (b.blockRef.mem && !b.blockRef.mem->requestAccess(range, Memory::Access::READ)) {
            throw "should creat view when accessable";
        }
    };

    auto* b = &(mLog->mBlocks[mFrom.blockIndex]);
    auto range = bytesRange(*b, mFrom.lineIndex, BLOCK_LINE_NUM);
    lock(*b, range);

    for (size_t i = mFrom.blockIndex+1; i < mTo.blockIndex; i++)
    {
        b = &(mLog->mBlocks[i]);
        range = bytesRange(*b, 0, BLOCK_LINE_NUM);
        lock(*b, range);
    }

    b = &(mLog->mBlocks[mTo.blockIndex]);
    range = bytesRange(*b, 0, mTo.lineIndex);
    lock(*b, range);
}

SubLogView::~SubLogView() {
    auto unlock = [](FilterBlock& b, Range& range){
        if (b.blockRef.mem) {b.blockRef.mem->unlock(range, Memory::Access::READ);}
    };
    
    auto* b = &(mLog->mBlocks[mFrom.blockIndex]);
    auto range = bytesRange(*b, mFrom.lineIndex, BLOCK_LINE_NUM);
    unlock(*b, range);

    for (size_t i = mFrom.blockIndex+1; i < mTo.blockIndex; i++)
    {
        b = &(mLog->mBlocks[i]);
        range = bytesRange(*b, 0, BLOCK_LINE_NUM);
        unlock(*b, range);
    }

    b = &(mLog->mBlocks[mTo.blockIndex]);
    range = bytesRange(*b, 0, mTo.lineIndex);
    unlock(*b, range);
}

LineRef SubLogView::current() const {
    auto& filterBlock = mLog->mBlocks[mCur.blockIndex];
    // LOGI("access %p %u", filterBlock.blockRef.block, mCur.blockIndex);
    auto lineIndexInBlock = filterBlock.lines[mCur.lineIndex];

    return {
        filterBlock.blockRef,
        &(filterBlock.blockRef.block->lines[lineIndexInBlock]),
        lineIndexInBlock
    };
}

void SubLogView::next() {
    if (mReverse) {
        --(mCur.lineIndex);
        if (mCur.lineIndex < 0) {
            --(mCur.blockIndex);
            if (mCur.blockIndex >= 0) {
                mCur.blockIndex = LastIndex(mLog->mBlocks[mCur.blockIndex].lines);
            }
        }
    } else {
        ++(mCur.lineIndex);
        if (mCur.lineIndex > LastIndex(mLog->mBlocks[mCur.blockIndex].lines)) {
            mCur.lineIndex = 0;
            ++(mCur.blockIndex);
        }
    }
}

bool SubLogView::end() {
    if (mReverse) {
        return mCur.blockIndex < mFrom.blockIndex ||
            (mCur.blockIndex == mFrom.blockIndex && mCur.lineIndex < mFrom.lineIndex);
    } else {
        return mCur.blockIndex > mTo.blockIndex || 
            (mCur.blockIndex == mTo.blockIndex && mCur.lineIndex > mTo.lineIndex);
    }
}

LogLineI SubLogView::size() const {
    return mCount;
}

shared_ptr<LogView> SubLogView::subview(LogLineI from, LogLineI n) const {
    auto fromPos = locateLine(from);
    auto toPos = locateLine(from + n - 1);
    
    if (mReverse){
        auto ret = make_shared<SubLogView>(mLog, toPos, fromPos, n);
        ret->mReverse = true;
    } 
    
    return make_shared<SubLogView>(mLog, fromPos, toPos, n);
}

SubLogPos SubLogView::locateLine(LogLineI line) const {
    LogLineI cur = 0;
    auto target = line + 1;

    if (mReverse) {
        size_t blockIndex = mTo.blockIndex;
        cur += mTo.lineIndex+1;

        while (cur < target && blockIndex > mFrom.blockIndex) {
            cur += mLog->mBlocks[--blockIndex].lines.size();
        }

        auto lineIndex = cur - target;

        if (blockIndex == mFrom.blockIndex && lineIndex < mFrom.lineIndex) {
            lineIndex = mFrom.lineIndex;
        }

        return {blockIndex, lineIndex};
    } else {
        size_t blockIndex = 0;
        cur += mLog->mBlocks[mFrom.blockIndex].lines.size() - mFrom.lineIndex;
        auto lastBlock = mTo.blockIndex;

        while (cur < target && blockIndex < lastBlock) {
            cur += mLog->mBlocks[++blockIndex].lines.size();
        }

        // 看看超出了多少，需要回吐
        auto lineIndex = LastIndex(mLog->mBlocks[blockIndex].lines) - (cur - target);

        //修正最后一个块索引可能超出的情况
        if (blockIndex == lastBlock && lineIndex > mTo.lineIndex) {
            lineIndex = mTo.lineIndex;
        }
        
        return {blockIndex, lineIndex};
    }

    return {0, 0};
}