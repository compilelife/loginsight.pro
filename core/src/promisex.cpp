#include "promisex.h"
#include <algorithm>
#include "eventloop.h"

void PromiseThenWrap::operator()(shared_ptr<Promise> p) {
    if (runOnMainThread) {
        auto handlerCopy = handler;
        EventLoop::instance().runOnMain([=]{
            handlerCopy(p);
        });
    } else {
        handler(p);
    }
}

shared_ptr<Promise> Promise::from(shared_ptr<promise<void>> p) {
    shared_ptr<Promise> ret(new Promise);

    auto p1 = make_shared<promise<void>>();
    ret->mEndFuture = p1->get_future();

    //用thread以避免async的诡异行为:
    //async返回的future会在其析构时（Promise析构时析构future）wait async内创建出来的thread
    //而如果调用者丢弃promise，比如post后不要返回值，就会出现post变成了“同步的”
    thread([ret, p, p1]()mutable{
        p->get_future().wait();
        
        lock_guard<mutex> l(ret->mThenLock);
        ret->mEnd = true;
        for (auto &&f : ret->mThens)
            f(ret->shared_from_this());

        p1->set_value();
    }).detach();

    return ret;
}

shared_ptr<Promise> Promise::from(function<any(bool*)>&& task) {
    shared_ptr<Promise> ret(new Promise);

    auto p = make_shared<promise<void>>();
    ret->mEndFuture = p->get_future();

    thread([ret, task, p]{
        ret->mResult = task(&ret->mIsCancelled);

        lock_guard<mutex> l(ret->mThenLock);
        ret->mEnd = true;
        for (auto &&f : ret->mThens)
            f(ret->shared_from_this());
        
        p->set_value();
    }).detach();

    return ret;
}

Promise::~Promise() {
    //不应该在析构的时候wait，比如post经常不要返回值，就会卡死在那里
    // mEndFuture.wait();
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
    return Promise::from([others](bool* cancel) mutable {
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
    });
}

Promise::Promise() {
    
}

shared_ptr<Promise> Promise::resolved(any&& v) {
    shared_ptr<Promise> p(new Promise);
    p->mEnd = true;
    p->mResult = v;
    //设置一个在函数结束时立即有效的end future，确保其他有用到end future的地方正常执行
    p->mEndFuture = async(launch::deferred, []{});
    return p;
}