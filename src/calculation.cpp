#include "calculation.h"
#include <thread>
#include <chrono>
#include <utility>
#include <algorithm>
#include "eventloop.h"
#include "stdout.h"

Calculation::Calculation() {
    mCoreNum = thread::hardware_concurrency();
}

Calculation& Calculation::instance() {
    static Calculation impl;
    return impl;
}

template<class Tasks, class ProcessFunc>
unique_ptr<Promise> scheduleImpl(Tasks&& tasks, ProcessFunc&& process) {
    return unique_ptr<Promise>(new Promise([tasks, process](bool* cancel)mutable{
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

unique_ptr<Promise> Calculation::schedule(shared_ptr<LogView> iter, 
            function<any(bool*,shared_ptr<LogView>)>&& process) {
    auto tasks = split(iter);
    return scheduleImpl(tasks, process);
}

unique_ptr<Promise> Calculation::schedule(vector<string_view> tasks, 
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