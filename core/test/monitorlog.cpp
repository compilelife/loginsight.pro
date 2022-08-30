#include "monitorlog.h"
#include "gtest/gtest.h"
#include <thread>
#include "eventloop.h"
#include <future>
#include "stdout.h"

using namespace std;
using namespace std::chrono_literals;

class MonitorLogTest : public testing::Test {
protected:
    event_base* evBase;
    MonitorLog log;

public:
    virtual void SetUp() {
        thread([]{EventLoop::instance().start();}).detach();
        evBase = EventLoop::instance().base();
    }

    // void runFor(long ms) {
    //     timeval tv;
    //     tv.tv_sec = ms / 1000;
    //     tv.tv_usec = (ms % 1000) * 1000;
    //     event_base_once(evBase, -1, EV_TIMEOUT, [](evutil_socket_t, short, void* arg){
    //         event_base_loopbreak((event_base*)arg);
    //     }, evBase, &tv);

    //     event_base_loop(evBase, EVLOOP_NO_EXIT_ON_EMPTY);
    // }

    // static void checkExitCb(evutil_socket_t,short,void* arg) {
    //     auto thiz = (MonitorLogTest*)arg;
    //     if (thiz->log.isAttrAtivated(LOG_ATTR_MAY_DISCONNECT)) {
    //         evtimer_del(thiz->timer);
    //         event_free(thiz->timer);
    //         event_base_loopbreak(thiz->evBase);
    //     } else {
    //         timeval tv {0, 20000};
    //         evtimer_add(thiz->timer, &tv);
    //     }
    // }

    // void runUntilExited() {
    //     timer = evtimer_new(evBase, &MonitorLogTest::checkExitCb, this);
    //     timeval tv {0, 20000};
    //     evtimer_add(timer, &tv);

    //     event_base_loop(evBase, EVLOOP_NO_EXIT_ON_EMPTY);
    // }

    virtual void TearDown() {
        EventLoop::instance().stop();
    }
};

TEST_F(MonitorLogTest, open) {
    string_view cmdline = "echo 1; sleep 0.5s;echo 2; sleep 0.5s;echo 3; sleep 0.5s;echo 4; sleep 0.5s;";

    ASSERT_TRUE(log.open(cmdline, evBase));

    this_thread::sleep_for(2500ms);

    ASSERT_EQ(4, log.range().len());

    log.close();
}

TEST_F(MonitorLogTest, close) {
    string_view cmdline = "for i in {1..40};do echo $i; sleep 0.5s;done;";

    ASSERT_TRUE(log.open(cmdline, evBase));

    this_thread::sleep_for(500ms);

    log.close();//要能立即关掉
}

TEST_F(MonitorLogTest, oneBlock) {
    string_view cmdline = "cat 204800.log";

    ASSERT_TRUE(log.open(cmdline, evBase));

    while (!log.isAttrAtivated(LOG_ATTR_MAY_DISCONNECT))
        this_thread::sleep_for(100ms);
    
    ASSERT_EQ(1720, log.range().len());

    log.close();
}

TEST_F(MonitorLogTest, dropBlock) {
    string_view cmdline = "cat 204898.log";

    log.setMaxBlockCount(1);
    ASSERT_TRUE(log.open(cmdline, evBase));

    while (!log.isAttrAtivated(LOG_ATTR_MAY_DISCONNECT))
        this_thread::sleep_for(100ms);

    Range expected{1720, 1721};//文件一共1722行，前1720行放一个block，最后两行放到新的block
    ASSERT_EQ(expected, log.range());

    log.close();
}