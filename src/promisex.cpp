#include "promisex.h"

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