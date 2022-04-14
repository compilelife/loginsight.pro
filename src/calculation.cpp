#include "calculation.h"
#include <thread>
#include <chrono>
#include <utility>
#include <algorithm>
#include "eventloop.h"
#include "stdout.h"
#include <regex>

Calculation::Calculation() {
    mCoreNum = thread::hardware_concurrency();
}

Calculation& Calculation::instance() {
    static Calculation impl;
    return impl;
}

template<class Tasks, class ProcessFunc>
shared_ptr<Promise> scheduleImpl(Tasks&& tasks, ProcessFunc&& process) {
    return shared_ptr<Promise>(new Promise([tasks, process](bool* cancel)mutable{
        vector<future<any>> futures;
        for (auto &&task : tasks) {
            auto futureRet = async(launch::async, process, cancel, move(task));
            futures.push_back(move(futureRet));
        }

        vector<any> rets;
        for (auto &&f : futures)
            rets.push_back(f.get());
        return rets;
    }));
}

shared_ptr<Promise> Calculation::schedule(shared_ptr<LogView> iter, 
            function<any(bool*,shared_ptr<LogView>)>&& process) {
    auto tasks = split(iter);
    return scheduleImpl(tasks, process);
}

shared_ptr<Promise> Calculation::schedule(vector<string_view> tasks, 
            function<any(bool*,string_view)>&& process) {
    return scheduleImpl(tasks, process);
}

vector<shared_ptr<LogView>> Calculation::split(shared_ptr<LogView>& iter) {
    auto size = iter->size();
    if (size <= BLOCK_LINE_NUM) {
        vector<shared_ptr<LogView>> ret;
        ret.push_back(move(iter));
        return ret;
    }

    LogLineI step = size / mCoreNum;
    LogLineI from = 0;

    vector<shared_ptr<LogView>> ret;

    while (from < size) {
        auto newIter = iter->subview(from, step);
        from += newIter->size();

        ret.push_back(move(newIter));
    }
    
    return ret;
}

pair<FindLineIter, FindLineIter> findLine(FindLineIter from, FindLineIter end) {
    auto newline = find(from, end, '\n');
    auto next = newline + 1;
    if (next > end)
        next = end;

    if (newline != end && newline != from && *(newline - 1) == '\r') {
        --newline;
    }

    return {newline, next};
}

struct StringIncase {
    string pattern;
    bool operator() (string_view text) {
        return text.find(pattern) != string_view::npos;
    }
};

struct StringCase {
    regex p;
    bool operator() (string_view text) {
        return regex_search(text.begin(), text.end(), p);
    }
};

FilterFunction createFilter(string_view pattern, bool caseSensitive) {
    if (caseSensitive) {
        return StringCase{regex(pattern.data(), regex::icase)};
    }
    return StringIncase {pattern.data()};
}