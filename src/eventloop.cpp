#include "eventloop.h"
#include "event2/thread.h"

void cppLambdaWrap(evutil_socket_t, short, void *arg) {
    auto callback = (EventHandler*)arg;
    (*callback)();
    delete callback;
}

EventLoop& EventLoop::instance() {
    static EventLoop impl;
    return impl;
}

EventLoop::EventLoop() {
#ifdef EVTHREAD_USE_PTHREADS_IMPLEMENTED
    evthread_use_pthreads();
#elif EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED
    evthread_use_windows_threads();
#endif

    mEventBase = event_base_new();
}

void EventLoop::start() {
    event_base_loop(mEventBase, EVLOOP_NO_EXIT_ON_EMPTY);
}

void EventLoop::stop() {
    event_base_loopexit(mEventBase, nullptr);
}

void EventLoop::post(EventHandler callback) {
    event_base_once(mEventBase, 
                    -1, 
                    EV_TIMEOUT, 
                    cppLambdaWrap, 
                    new EventHandler(callback), nullptr);
}