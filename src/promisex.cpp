#include "promisex.h"
#include <algorithm>

Promise::Promise(function<any(bool*)>&& task) {
    mEndFuture = async(launch::async, [this, task]{
        this->mResult = task(&this->mIsCancelled);

        lock_guard<mutex> l(this->mThenLock);
        mEnd = true;
        for (auto &&f : this->mThens)
            f(*this);
    });
}

void Promise::cancel() {
    mIsCancelled = true;
    wait();
}

bool Promise::wait(long ms) {
    if (ms == 0) {
        mEndFuture.wait();
        return true;
    }

    return mEndFuture.wait_for(chrono::milliseconds(ms)) == future_status::ready;
}

bool Promise::isBusy() {
    return !mEnd;
}

void Promise::then(function<void(Promise&)>&& handler) {
    lock_guard<mutex> l(mThenLock);
    if (mEnd) {
        handler(*this);
    } else {
        mThens.push_back(handler);
    }
}

unique_ptr<Promise> Promise::all(vector<shared_ptr<Promise>> others) {
    return unique_ptr<Promise>(new Promise([others](bool* cancel) mutable {
        auto begin = others.begin();
        auto end = others.end();
        bool allReady = false;

        do {
            allReady = all_of(begin, end, 
                [](shared_ptr<Promise>& p){return p->wait(10);});
        } while(!allReady && !*cancel);

        if (cancel) {
            for_each(begin, end,
                [](shared_ptr<Promise>& p){p->cancel();});
        } else {
            vector<any> results;
            for_each(begin, end, 
                [&results](shared_ptr<Promise>& p){results.push_back(p->value());});
            return results;
        }

        return vector<any>();
    }));
}