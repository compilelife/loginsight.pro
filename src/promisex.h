#pragma once

#include <any>
#include "def.h"
#include <functional>
#include <atomic>
#include <future>
#include <vector>
#include <mutex>
#include <memory>

class Promise;
using PromiseThen=function<void(shared_ptr<Promise>)>;

struct PromiseThenWrap {
    bool runOnMainThread;
    PromiseThen handler;

    void operator()(shared_ptr<Promise> p);
};

class Promise : public enable_shared_from_this<Promise>{
private:
    future<void> mEndFuture;
    bool mIsCancelled{false};

    mutex mThenLock;
    any mResult;
    bool mEnd{false};
    vector<PromiseThen> mThens;
private:
    Promise();
public:
    Promise(function<any(bool*)>&& task);
    ~Promise();
    static shared_ptr<Promise> all(vector<shared_ptr<Promise>> others);
    static shared_ptr<Promise> resolved(any&& v);
public:
    void cancel();
    bool isCancelled() {return mIsCancelled;}
    bool wait(long ms = 0);
    bool isBusy();
public:
    void then(PromiseThen&& handler, bool runOnMainThread = false);
    void setValue(any&& v) {mResult = v;}
public:
    any& value() {return mResult;}
    vector<any> calculationValue() const {return any_cast<vector<any>>(mResult);};
};