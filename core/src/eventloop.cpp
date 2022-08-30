#include "eventloop.h"
#include "event2/thread.h"
#include "stdout.h"

/**
 * EventLoop机制：
 * - 内部有10ms定时器(drainQueue)，不断检查mEventQueue是否有任务，如有则执行
 * - 所有任务都会返回promise
 * - 执行read任务期间可以继续执行其他read任务，但不能执行write任务；write任务执行期间，不能执行read/write任务
 * - 通过post提交任务到任务队列
 * - 其他直接由event_add添加到线程的任务，由libevent调度，穿插在drainQueue之间执行
 */

struct CppLambda {
    function<void()> handler;
};

void cppLambdaWrap(evutil_socket_t, short, void *arg) {
    auto lambda = (CppLambda*)arg;
    lambda->handler();
    delete lambda;
}

EventLoop& EventLoop::instance() {
    static EventLoop impl;
    return impl;
}

EventLoop::EventLoop() {
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(0x0201, &wsa_data);
#endif

#ifdef EVTHREAD_USE_PTHREADS_IMPLEMENTED
    evthread_use_pthreads();
#elif EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED
    evthread_use_windows_threads();
#endif

    mEventBase = event_base_new();

    mThdId = this_thread::get_id();
    mDrainEvent = event_new(mEventBase, -1, EV_PERSIST,
    [](evutil_socket_t, short, void* arg){
        auto thiz = (EventLoop*)arg;
        thiz->drainQueue();
    }, this);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;//10ms
    evtimer_add(mDrainEvent, &tv);
}

void EventLoop::start() {
    mRunEventType = EventType::None;
    mRunCount = 0;
    event_base_loop(mEventBase, EVLOOP_NO_EXIT_ON_EMPTY);
}

void EventLoop::stop() {
    event_base_loopexit(mEventBase, nullptr);
    while(!mEventQueue.empty())
        mEventQueue.pop();
}

void EventLoop::runOnMain(function<void()> callback) {
    event_base_once(mEventBase,
                    -1,
                    EV_TIMEOUT,
                    cppLambdaWrap,
                    new CppLambda{move(callback)}, nullptr);
}

shared_ptr<Promise> EventLoop::runEvent(EventType type, EventHandler callback) {
    auto p = callback();
    ++mRunCount;
    mRunEventType = type;

    p->then([this, type](shared_ptr<Promise> p){
            lock_guard<recursive_mutex> l(mLock);
            if (--mRunCount <= 0) {
                mRunEventType = EventType::None;
            }
            drainQueue();
        });
    
    return p;
}

void EventLoop::drainQueue() {
    lock_guard<recursive_mutex> l(mLock);
    if (mEventQueue.empty())
        return;

    auto e = mEventQueue.front();
    auto canRun = mRunEventType == EventType::None
                     || mRunEventType == EventType::Read && e.type == EventType::Read;

    if (!canRun)
        return;

    mEventQueue.pop();
    runEvent(e.type, e.handler);
}

bool EventLoop::canWrite() {
    lock_guard<recursive_mutex> l(mLock);
    return mRunEventType == EventType::None;
}

shared_ptr<Promise> EventLoop::post(EventType type, EventHandler callback) {
    lock_guard<recursive_mutex> l(mLock);

    auto immediateRun = mThdId == this_thread::get_id()
                     && (mRunEventType == EventType::None
                     || mRunEventType == EventType::Read && type == EventType::Read);
    
    if (immediateRun) {
        return runEvent(type, callback);
    } else {
        auto p = make_shared<promise<void>>();
        auto ret = Promise::from(p);
        mEventQueue.push({type, [callback, p]{
            auto ret = callback();
            ret->then([p](shared_ptr<Promise>){p->set_value();}, false);
            return ret;
        }});
        return ret;
    }
}