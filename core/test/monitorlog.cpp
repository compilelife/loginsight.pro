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
    thread evThd;

public:
    virtual void SetUp() {
        evThd = thread([]{EventLoop::instance().start();});
        evBase = EventLoop::instance().base();
    }

    virtual void TearDown() {
        EventLoop::instance().stop();
        evThd.join();
    }
};

TEST_F(MonitorLogTest, open) {
#ifdef _WIN32
    string_view cmdline = "echo 1 & echo 2 & echo 3";//timeout会输出额外的行，先不sleep
#else
    string_view cmdline = "echo 1; sleep 1;echo 2; sleep 0.5;echo 3;";
#endif

    ASSERT_TRUE(log.open(cmdline, evBase));

    this_thread::sleep_for(2500ms);

    ASSERT_EQ(3, log.range().len());

    log.close();
}

TEST_F(MonitorLogTest, close) {
#ifdef _WIN32
    string_view cmdline = "timeout /t 1 & timeout /t 1";
#else
    string_view cmdline = "sleep 1; sleep 1";
#endif

    ASSERT_TRUE(log.open(cmdline, evBase));

    this_thread::sleep_for(500ms);

    log.close();//要能立即关掉
}

TEST_F(MonitorLogTest, oneBlock) {
#ifdef _WIN32
    string_view cmdline = "type 204800.log";
#else
    string_view cmdline = "cat 204800.log";
#endif

    ASSERT_TRUE(log.open(cmdline, evBase));

    while (!log.isAttrAtivated(LOG_ATTR_MAY_DISCONNECT))
        this_thread::sleep_for(100ms);
    
    ASSERT_EQ(1720, log.range().len());

    log.close();
}

TEST_F(MonitorLogTest, dropBlock) {//TODO
#ifdef _WIN32
    string_view cmdline = "type 204898.log";
#else
    string_view cmdline = "cat 204898.log";
#endif

    log.setMaxBlockCount(1);
    ASSERT_TRUE(log.open(cmdline, evBase));

    while (!log.isAttrAtivated(LOG_ATTR_MAY_DISCONNECT))
        this_thread::sleep_for(100ms);

    Range expected{1720, 1721};//文件一共1722行，前1720行放一个block，最后两行放到新的block
    ASSERT_EQ(expected, log.range());

    log.close();
}