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

bool Calculation::isBusy() {
    return !mEnd;
}

void Calculation::cancel() {
    mCanceled = true;
    wait();
}

bool Calculation::wait(long ms) {
    if (ms == 0) {
        mEndFuture.wait();
        return true;
    }

    return mEndFuture.wait_for(chrono::milliseconds(ms)) == future_status::ready;
}

void Calculation::prepareSchedule() {
    mTaskFutures.clear();
    mThens.clear();
    mFinals.clear();

    mCanceled = false;
    mEnd = false;
}

Calculation& Calculation::sealed() {
    mEndFuture = async(launch::async, [this]mutable{
        vector<any> ret;

        for (auto &&f : this->mTaskFutures)
            ret.push_back(f.get());
        
        if (!this->isCancelled()) {
            for (auto &&handler : this->mThens)
                handler(ret);
        }

        for (auto &&handler : this->mFinals)
            handler();

        mEnd = true;
    });
    return *this;
}

Calculation& Calculation::schedule(unique_ptr<LogView> iter, 
            function<any(unique_ptr<LogView>)>&& process) {
    auto tasks = split(move(iter));

    prepareSchedule();

    for (auto &&task : tasks) {
        auto futureRet = async(launch::async, process, move(task));
        mTaskFutures.push_back(move(futureRet));
    }

    return *this;
}

Calculation& Calculation::schedule(vector<string_view> tasks, 
            function<any(string_view)>&& process) {
    prepareSchedule();

    for (auto &&task : tasks) {
        auto futureRet = async(launch::async, process, move(task));
        mTaskFutures.push_back(move(futureRet));
    }
    
    return *this;
}

vector<unique_ptr<LogView>> Calculation::split(unique_ptr<LogView>&& iter) {
    auto size = iter->size();
    if (size <= BLOCK_LINE_NUM) {
        vector<unique_ptr<LogView>> ret;
        ret.push_back(move(iter));
        return ret;
    }

    LogLineI step = size / mCoreNum;
    LogLineI from = 0;

    vector<unique_ptr<LogView>> ret;

    while (from < size) {
        auto newIter = iter->subview(from, step);
        from += newIter->size();

        ret.push_back(move(newIter));
    }
    
    return ret;
}

Calculation& Calculation::then(ThenFunc&& handler) {
    mThens.push_back(handler);
    return *this;
}

Calculation& Calculation::finally(FinalFunc&& atLast) {
    mFinals.push_back(atLast);
    return *this;
}