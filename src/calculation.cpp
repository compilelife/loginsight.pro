#include "calculation.h"
#include <thread>
#include <chrono>
#include <utility>
#include <algorithm>
#include "eventloop.h"
#include "stdout.h"
#include <regex>
#include <list>

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

shared_ptr<Promise> Calculation::peekFirst(shared_ptr<LogView> iter, 
            function<any(bool*,shared_ptr<LogView>)>&& process) {
    return shared_ptr<Promise>(new Promise([tasks = split(iter), process](bool* cancel){
        list<shared_ptr<Promise>> futures;
        for (auto &&task : tasks) {
            //转换为promise，以方便在获得首个结果后可以cancel之
            auto futureRet = shared_ptr<Promise>(new Promise([task, process](bool* c){
                return process(c, task);
            }));
            futures.push_back(move(futureRet));
        }

        //按入队顺序依次取线程的返回值，如果有值，则返回，否则继续wait
        while (!futures.empty()) {
            futures.front()->wait();
            auto ret = futures.front()->value();

            futures.pop_front();

            if (ret.has_value()) {
                //等待其他计算线程退出，避免这些线程仍在后台执行的情况下，与主线程的其他计算形成多线程竞争
                for (auto &&p : futures)
                    p->cancel();
                
                return ret;
            }
        }
        
        return any();
    }));
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

struct RegexMatch {
    regex p;
    bool operator() (string_view text) {
        return regex_search(text.begin(), text.end(), p);
    }
};

FilterFunction createFilter(string_view pattern, bool caseSensitive) {
    if (caseSensitive) {
        return RegexMatch{regex(pattern.data(), regex::icase)};
    }
    return StringIncase {pattern.data()};
}

FilterFunction createFilter(regex r) {
    return RegexMatch{r};
}