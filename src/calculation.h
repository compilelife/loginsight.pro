#pragma once

#include "def.h"
#include "logview.h"
#include <functional>
#include <any>
#include "promisex.h"
#include <string_view>

using CalculationRet = vector<any>;

class Calculation {
private:
    unsigned mCoreNum;
private:
    Calculation();
    vector<shared_ptr<LogView>> split(shared_ptr<LogView>& iter);
public:
    static Calculation& instance();
    unsigned coreNum() {return mCoreNum;}

public:
    unique_ptr<Promise> schedule(shared_ptr<LogView> iter, 
            function<any(bool*,shared_ptr<LogView>)>&& process);

    unique_ptr<Promise> schedule(vector<string_view> tasks, 
            function<any(bool*,string_view)>&& process);
};

template<class T>
void consumeCalculation(CalculationRet&& ret, function<void(T&&)>&& consumer) {
    for (auto &&a : ret)
    {
        consumer(any_cast<T>(a));
    }
}

using FilterFunction = function<bool(string_view)> ;
FilterFunction createFilter(string_view pattern, bool caseSensitive);

/**
 * @brief 对string_view从from到end（不含）查找换行位置
 * @returns pair<换行位置(\r或\n或end)，下一次检索位置>
 */
using FindLineIter = string_view::const_iterator;
pair<FindLineIter, FindLineIter> findLine(FindLineIter from, FindLineIter end);

