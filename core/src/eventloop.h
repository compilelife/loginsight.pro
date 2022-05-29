#pragma once

#include <event2/event.h>
#include "def.h"
#include <functional>
#include <queue>
#include <map>
#include "promisex.h"
#include <thread>


using EventHandler = function<shared_ptr<Promise>()>;
enum EventType {
    None,
    Immediate,
    Write,
    Read
};
struct Event {
    EventType type;
    EventHandler handler;
};

//设计队列机制如下：
//post:
//if has write task running
//  push to queue
//else 
//  if type is write event
//      push to queue
//  if type is read event
//      run it
//
//add 10ms check task:
//if queue is empty or has write task running
//  return
//else
//  peek head
//  if type is write event
//      push to queue
//  if type is read event
//      run it
//
//on stdin:
//  if type is immediate event
//      processing
//  else
//      post
//
//handler onProcess:
//  set event done on finish
//
//如果是一个耗时事件，我们需要启动一个Promise（可取消），怎么与EventLoop打通？
//  ——EventHandler需要返回一个Promise，对于不需要开线程处理的，直接返回resolved即可；需要线程处理的，返回它的Promise
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
    shared_ptr<Promise> post(EventType type, EventHandler callback);
    void runOnMain(function<void()> callback);
    void drainQueue();

    bool canWrite();

private:
    shared_ptr<Promise> runEvent(EventType type, EventHandler callback);//call with lock

private:
    queue<Event> mEventQueue;
    EventType mRunEventType{EventType::None};
    int mRunCount{0};
    recursive_mutex mLock;
    thread::id mThdId;
    event* mDrainEvent;
};