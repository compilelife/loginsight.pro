#pragma once
#include "event2/event.h"
#include <memory>
#include "def.h"
#include <functional>

class PingTask : public enable_shared_from_this<PingTask>{
private:
    PingTask(function<bool()> predict, function<bool()> handler);
    timeval interval();
    static void eventCallback(evutil_socket_t fd, short flags, void* arg);
public:
    ~PingTask();
    void start(event_base* evbase, long ms);
    void stop();
public:
    static shared_ptr<PingTask> create(function<bool()>&& predict, function<bool()>&& handler);
private:
    event* mEv {nullptr};
    long mInterval;
    function<bool()> mPredict;
    function<bool()> mHandler;
    bool mCancel{false};
};