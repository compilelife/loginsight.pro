#include "sublogview.h"
#include "sublog.h"

SubLogView::SubLogView(const SubLog* log)
    :mLog(const_cast<SubLog*>(log)) {
    mFrom = {0,0};
    mTo = {LastIndex(log->mBlocks), LastIndex(LastItem(log->mBlocks).lines)};
    mCur = mFrom;
    mCount = log->mCount;
}

SubLogView::SubLogView(SubLog* log, SubLogPos from, SubLogPos to, LogLineI n) 
    :mLog(log), mFrom(from), mTo(to), mCur(mFrom), mCount(n) {

}

LineRef SubLogView::current() const {
    auto& filterBlock = mLog->mBlocks[mCur.blockIndex];
    auto lineIndexInBlock = filterBlock.lines[mCur.lineIndex];

    return {
        filterBlock.blockRef,
        &(filterBlock.blockRef.block->lines[lineIndexInBlock]),
        lineIndexInBlock
    };
}

void SubLogView::next() {
    ++(mCur.lineIndex);
    if (mCur.lineIndex > LastIndex(mLog->mBlocks[mCur.blockIndex].lines)) {
        mCur.lineIndex = 0;
        ++(mCur.blockIndex);
    }
}

bool SubLogView::end() {
    return mCur.blockIndex > LastIndex(mLog->mBlocks) || 
          (mCur.blockIndex == LastIndex(mLog->mBlocks) &&
           mCur.lineIndex > mTo.lineIndex);
}

LogLineI SubLogView::size() const {
    return mCount;
}

shared_ptr<LogView> SubLogView::subview(LogLineI from, LogLineI n) const {
    auto fromPos = locateLine(from);
    auto toPos = locateLine(from + n - 1);
    return make_shared<SubLogView>(mLog, fromPos, toPos, n);
}

SubLogPos SubLogView::locateLine(LogLineI line) const {
    LogLineI cur = 0;
    auto target = line + 1;

    size_t blockIndex = 0;
    cur += mLog->mBlocks[mFrom.blockIndex].lines.size() - mFrom.lineIndex;
    auto lastBlock = LastIndex(mLog->mBlocks);

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