#pragma once

#include "def.h"
#include "logview.h"
#include <functional>
#include <any>
#include "promisex.h"
#include <string_view>
#include <regex>

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
    shared_ptr<Promise> schedule(shared_ptr<LogView> iter, 
            function<any(bool*,shared_ptr<LogView>)>&& process);

    shared_ptr<Promise> schedule(vector<string_view> tasks, 
            function<any(bool*,string_view)>&& process);

    shared_ptr<Promise> peekFirst(shared_ptr<LogView> iter, 
            function<any(bool*,shared_ptr<LogView>)>&& process);

public:
    template<class T>
    static vector<T> flat(CalculationRet&& ret);
};

template<class T>
void consumeCalculation(CalculationRet&& ret, function<void(T&&)>&& consumer) {
    for (auto &&a : ret)
    {
        consumer(any_cast<T>(a));
    }
}

template<class T>
vector<T> Calculation::flat(CalculationRet&& ret) {
    vector<T> flatten;
    for (auto &&i : ret) {
        auto vi = any_cast<vector<T>>(i);
        copy(vi.begin(), vi.end(), back_inserter(flatten));
    }
    return flatten;
}

using FilterFunction = function<bool(string_view)> ;
FilterFunction createFilter(string_view pattern, bool caseSensitive);
FilterFunction createFilter(regex pattern);

struct FindRet
{
    LineCharI offset{0};//匹配词在行的位置
    LineCharI len{0};//匹配词的长度
    operator bool() const{
        return len > 0;
    }
    bool operator == (const FindRet& o) {
        return o.offset == offset && o.len == len;
    }
    static FindRet failed() {
        return FindRet();
    }
};

using FindFunction = function<FindRet(string_view)>;
FindFunction createFind(string_view pattern, bool caseSensitive, bool reverse);
FindFunction createFind(regex pattern, bool reverse);

/**
 * @brief 对string_view从from到end（不含）查找换行位置
 * @returns pair<换行位置(\r或\n或end)，下一次检索位置>
 */
using FindLineIter = string_view::const_iterator;
pair<FindLineIter, FindLineIter> findNewLine(FindLineIter from, FindLineIter end);

