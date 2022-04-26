#include "stdout.h"
#include "pingtask.h"
#include "eventloop.h"

PingTask::PingTask(function<bool()> predict, function<bool()> handler) 
    :mPredict(predict), mHandler(handler) {

}

PingTask::~PingTask() {
    event_del_block(mEv);
    event_free(mEv);
}

timeval PingTask::interval() {
    timeval tv;
    tv.tv_sec = mInterval/1000;
    tv.tv_usec = (mInterval % 1000) * 1000;
    return tv;
}

void PingTask::start(long ms) {
    mInterval = ms;
    mEv = evtimer_new(EventLoop::instance().base(), 
                        &PingTask::eventCallback, 
                        new shared_ptr<PingTask>(shared_from_this()));

    timeval tv = interval();
    evtimer_add(mEv, &tv);
}

void PingTask::stop() {
    mCancel = true;
}

void PingTask::eventCallback(evutil_socket_t fd, short flags, void* arg) {
    auto* pthis = (shared_ptr<PingTask>*)(arg);
    auto& thiz = *pthis;

    if (thiz->mCancel) {
        delete pthis;
        return;
    }

    bool runNext = true;
    if (thiz->mPredict()) {
        runNext = thiz->mHandler();
    }

    if (runNext) {
        timeval tv = thiz->interval();
        event_add(thiz->mEv, &tv);
    } else {
        delete pthis;
    }
}

shared_ptr<PingTask> PingTask::create(function<bool()>&& predict, function<bool()>&& handler) {
    return shared_ptr<PingTask>(new PingTask(predict, handler));
}