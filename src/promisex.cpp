#include "promisex.h"
#include <algorithm>
#include "eventloop.h"

void PromiseThenWrap::operator()(shared_ptr<Promise> p) {
    if (runOnMainThread) {
        auto handlerCopy = handler;
        EventLoop::instance().post([=]{
            handlerCopy(p);
        });
    } else {
        handler(p);
    }
}

Promise::Promise(function<any(bool*)>&& task) {
    mEndFuture = async(launch::async, [thiz=shared_from_this(), task]{
        thiz->mResult = task(&thiz->mIsCancelled);

        lock_guard<mutex> l(thiz->mThenLock);
        thiz->mEnd = true;
        for (auto &&f : thiz->mThens)
            f(thiz);
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

void Promise::then(PromiseThen&& handler, bool runOnMainThread) {
    PromiseThenWrap wrap;
    wrap.handler = handler;
    wrap.runOnMainThread = runOnMainThread;

    lock_guard<mutex> l(mThenLock);
    if (mEnd) {
        wrap(shared_from_this());
    } else {
        mThens.push_back(wrap);
    }
}

shared_ptr<Promise> Promise::all(vector<shared_ptr<Promise>> others) {
    return shared_ptr<Promise>(new Promise([others](bool* cancel) mutable {
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