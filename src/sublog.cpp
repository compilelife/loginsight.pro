#include "sublog.h"
#include <algorithm>
#include <numeric>

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

unique_ptr<Promise> SubLog::createSubLog(shared_ptr<LogView> iter, FilterFunction predict) {
    auto filterTask = Calculation::instance().schedule(iter, 
                            bind(doFilter, placeholders::_1, placeholders::_2, predict));

    filterTask->then([](Promise& p) {
        if (p.isCancelled())
            return;
        
        auto blocksVec = p.calculationValue();

        p.setValue(assemble(move(blocksVec)));
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
    if (range() == Range{from, to}) {
        return view;
    }

    return view->subview(from, to - from + 1);
}