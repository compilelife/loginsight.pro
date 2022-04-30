#include "sublog.h"
#include <algorithm>
#include <numeric>
#include "stdout.h"

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

shared_ptr<Promise> SubLog::createSubLog(shared_ptr<LogView> iter, FilterFunction predict) {
    auto filterTask = Calculation::instance().schedule(iter, 
                            bind(doFilter, placeholders::_1, placeholders::_2, predict));

    filterTask->then([](shared_ptr<Promise> p) {
        if (p->isCancelled())
            return;
        
        auto blocksVec = p->calculationValue();

        p->setValue(assemble(move(blocksVec)));
    });

    return filterTask;
}

SubLog::SubLog(vector<FilterBlock>&& blocks, LogLineI count) 
    :mBlocks(blocks), mCount(count) {

}

Range SubLog::range() const {
    if (mCount == 0)
        return {};

    return {0, mCount - 1};
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
    return mBlocks[blockIndex].lines[lineIndex];
}

LogLineI SubLog::fromSource(LogLineI target) const {
    //第一次二分查找用来定位所在block
    size_t low = 0;
    size_t high = LastIndex(mBlocks);
    size_t mid;
    while (low <= high) {
        mid = (low + high) / 2;
        auto b = mBlocks[mid];
        if (b.lines[0] <= target && target >= LastItem(b.lines)) {
            break;
        } else if (target > LastItem(b.lines)) {
            low = mid + 1;
        } else if (target < b.lines[0]) {
            high = mid - 1;
        }
    }

    if (high > low) {
        return 0;//如果没有在任意block的范围内，则直接返回失败的默认值0
    }

    //第二次二分查找用来定位最接近的行
    auto block = mBlocks[mid];
    low = 0;
    high = LastIndex(block.lines);

    while (low <= high) {
        mid = (low + high) / 2;
        auto line = block.lines[mid];
        if (line == target) {
            return mid;
        } else if (target > line) {
            low = mid + 1;
        } else if (target < line) {
            high = mid - 1;
        }
    }
    
    return high;//如果没有找到，返回high，也就是刚好低于target的那行
}