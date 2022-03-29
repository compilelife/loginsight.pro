#pragma once

#include <any>
#include "def.h"
#include <functional>
#include <atomic>
#include <future>
#include <vector>
#include <mutex>

class Promise {
private:
    future<void> mEndFuture;
    bool mIsCancelled{false};

    mutex mThenLock;
    any mResult;
    bool mEnd{false};
    vector<function<void(Promise&)>> mThens;
public:
    Promise(function<any(bool*)>&& task);
public:
    void cancel();
    bool isCancelled() {return mIsCancelled;}
    bool wait(long ms = 0);
    bool isBusy();
public:
    void then(function<void(Promise&)>&& handler);
public:
    any& value() {return mResult;}
    vector<any> calculationValue() const {return any_cast<vector<any>>(mResult);};
};