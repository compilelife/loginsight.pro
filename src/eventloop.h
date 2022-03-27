#pragma once

#include <event2/event.h>
#include "def.h"
#include <functional>

using EventHandler = function<void()>;

class EventLoop {
private:
    event_base* mEventBase;
public:
    static EventLoop& instance();
    EventLoop();
    void start();
    void stop();
    void post(EventHandler callback);
};