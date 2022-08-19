#include "sublog.h"
#include <algorithm>
#include <numeric>
#include "stdout.h"

LogLineI FilterBlock::mapLineIndexToSource(BlockLineI i) const {
    return blockRef.block->lineBegin + lines[i];
}

Range FilterBlock::sourceLineRange() const {
    return Range{
        blockRef.block->lineBegin,
        blockRef.block->lineBegin + LastItem(lines)
    };
}

static vector<FilterBlock> doFilter(
                                    bool* cancel, 
                                    shared_ptr<LogView> view, 
                                    FilterFunction predict) {
    vector<FilterBlock> ret;
    FilterBlock* cur = nullptr;
    
    while (!view->end() && !*cancel) {
        auto refLine = view->current();
        if (predict(refLine.str())) {
            if (!cur || cur->blockRef.block != refLine.refBlock.block) {
                ret.push_back({refLine.refBlock});
                cur = &LastItem(ret);
            }
            cur->lines.push_back(refLine.indexInBlock);
        }
        view->next();
    }
    
    return ret;
}

static shared_ptr<SubLog> assemble(CalculationRet&& filterRet) {
    using RetType = vector<FilterBlock>;

    RetType ret;
    FilterBlock* last = nullptr;
    LogLineI count = 0;

    consumeCalculation<RetType>(move(filterRet), [&](RetType&& a){
        auto cur = a.begin();
        auto end = a.end();
        if (cur == end)
            return;

        for_each(cur, end, [&count](FilterBlock& b){count+=b.lines.size();});

        if (last && last->blockRef.block == cur->blockRef.block) {
            //胶合被砍断的block
            copy(cur->lines.begin(), cur->lines.end(), back_inserter(last->lines));
            ++cur;
        }

        copy(cur, end, back_inserter(ret));
        last = &LastItem(ret);
    });

    return make_shared<SubLog>(move(ret), count);
}

shared_ptr<Promise> SubLog::createSubLog(shared_ptr<ILog> parent, FilterFunction predict) {
    auto filterTask = Calculation::instance().schedule(parent->view(), 
                            bind(doFilter, placeholders::_1, placeholders::_2, predict));

    filterTask->then([parent, predict](shared_ptr<Promise> p) {
        if (p->isCancelled())
            return;
        
        auto blocksVec = p->calculationValue();

        auto subLog = assemble(move(blocksVec));
        subLog->mParent = parent;
        subLog->mMapRange = parent->range();
        subLog->mFilter = predict;
        p->setValue(subLog);
    }, false);

    return filterTask;
}

SubLog::SubLog(vector<FilterBlock>&& blocks, LogLineI count) 
    :mBlocks(blocks), mCount(count) {

}

Range SubLog::range() const {
    if (mCount == 0)
        return {};

    return {mMyFrom, mMyFrom + mCount - 1};
}

shared_ptr<LogView> SubLog::view(LogLineI from, LogLineI to) const {
    shared_ptr<SubLogView> view(new SubLogView(this));
    if (from == 0 && to == InvalidLogLine)
        return view;

    return view->subview(from, to - from + 1);
}

LogLineI SubLog::mapToSource(LogLineI index) const {
    //先逐个累加block的line个数，逼近index+1
    auto target = index + 1;
    LogLineI cur = 0;
    auto lastBlock = LastIndex(mBlocks);
    size_t blockIndex = 0;
    cur += mBlocks[blockIndex].lines.size();
    while (cur < target && blockIndex < lastBlock) {
        cur += mBlocks[++blockIndex].lines.size();
    }
    if (cur < target)
        return 0;
    
    //然后返回源的index
    auto lineIndex = LastIndex(mBlocks[blockIndex].lines) - (cur - target);
    return mBlocks[blockIndex].mapLineIndexToSource(lineIndex);
}

LogLineI SubLog::fromSource(LogLineI target) const {
    //第一次二分查找用来定位所在block
    int low = 0;
    int high = LastIndex(mBlocks);
    if (low > high) {
        return InvalidLogLine;
    }
    
    int mid = 0;
    while (low <= high) {
        mid = (low + high) / 2;
        auto& b = mBlocks[mid];
        auto range = b.sourceLineRange();
        if (range.contains(target)) {
            break;
        } else if (target > range.end) {
            low = mid + 1;
        } else if (target < range.begin) {
            high = mid - 1;
        }
    }

    LogLineI offset = 0;
    for (int i = 0; i < mid; i++)
        offset += mBlocks[i].lines.size();

    //第二次二分查找用来定位最接近的行
    auto& block = mBlocks[mid];
    if (low > high) {//没有一个block包含它
        //看看距离左边界还是右边界更近？    
        auto range = block.sourceLineRange();
        if (target < range.begin)
            return offset;
        else//target > range.end
            return offset + block.lines.size();
    }

    low = 0;
    high = LastIndex(block.lines);

    while (low <= high) {
        mid = (low + high) / 2;
        auto line = block.mapLineIndexToSource(mid);
        if (line == target) {
            return mid + offset;
        } else if (target > line) {
            low = mid + 1;
        } else if (target < line) {
            high = mid - 1;
        }
    }
    
    return mid + offset;//如果未找到，则返回最接近行
}

static pair<Range, Range> detectChange(Range old, Range now) {
    Range clip = now.begin == old.begin ? Range() : Range{old.begin, now.begin - 1};
    Range append = {old.end + 1, now.end};
    if  (append.begin < now.begin) {
        append.begin = now.begin;
    }
    return {clip, append};
}

void SubLog::syncParent() {
    auto curRange = mParent->range();
    auto [clip, append] = detectChange(mMapRange, curRange);

    LOGI("clip: %d,%d; append: %d,%d", clip.begin, clip.end, append.begin, append.end);

    if (clip.valid())
        dropByParentRange(clip);
    if (append.valid())
        appendByParentRange(append);

    mMapRange = curRange;
}

void SubLog::dropByParentRange(Range r) {
    //目前只可能从头开始drop，所以只需要drop到r.end即可
    auto it = mBlocks.begin();
    while (it != mBlocks.end()) {
        auto thisBlockLastLineInParent = LastItem(it->lines);
        
        if (r.end >= thisBlockLastLineInParent) {//drop整个block
            mCount -= it->lines.size();
            mMyFrom += it->lines.size();
            mBlocks.erase(it);
        } else {//落在当前block内，删除较小的行
            auto jt = it->lines.begin();
            while (jt != it->lines.end()) {
                if (*jt <= r.end) {
                    --mCount;
                    ++mMyFrom;
                    it->lines.erase(jt);
                } else {
                    ++jt;
                }
            }
            
            break;    
        }
        ++it;
    }
}

void SubLog::appendByParentRange(Range r) {
    auto view = mParent->view(r.begin, r.end);
    bool cancel = false;
    auto blocks = doFilter(&cancel, view, mFilter);
    if (blocks.empty())
        return;

    size_t count = 0;
    for_each(blocks.begin(), blocks.end(), [&count](FilterBlock& b){count+=b.lines.size();});
    mCount += count;

    //胶合block
    if (!mBlocks.empty()) {
        auto& lastOldBlock = LastItem(mBlocks);
        auto it = blocks.begin();
        auto& firstNewBlock = *it;
        if (lastOldBlock.blockRef.block == firstNewBlock.blockRef.block) {
            copy(firstNewBlock.lines.begin(), firstNewBlock.lines.end(), back_inserter(lastOldBlock.lines));
            ++it;
        }

        //放入剩余的block
        copy(it, blocks.end(), back_inserter(mBlocks));
    }
}