#pragma once

#include <event2/event.h>
#include "def.h"
#include <functional>

using EventHandler = function<void()>;

class EventLoop {
private:
    event_base* mEventBase;
    EventLoop();
public:
    static EventLoop& instance();
    event_base* base() {return mEventBase;};
    void start();
    void stop();

public:
    void post(EventHandler callback);
};