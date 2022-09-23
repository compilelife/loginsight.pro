#include "sublog.h"
#include "gtest/gtest.h"
#include "def.h"
#include "filelog.h"
#include "monitorlog.h"
#include "eventloop.h"
#include <memory>
#include <thread>

TEST(SubLog, create) {
    auto log = make_shared<FileLog>();
    log->open("sample.log");
    log->scheduleBuildBlocks()->wait();

    auto&& p = SubLog::createSubLog(log, createFilter("chromium", true));
    p->wait();

    auto sublog = any_cast<shared_ptr<SubLog>>(p->value());

    ASSERT_EQ(132852, sublog->range().len());
}

TEST(SubLog, subLogOfSubLog) {
    auto log = make_shared<FileLog>();
    log->open("sample.log");
    log->scheduleBuildBlocks()->wait();

    auto&& p = SubLog::createSubLog(log, createFilter("chromium", true));
    p->wait();

    auto sublog = any_cast<shared_ptr<SubLog>>(p->value());

    p = SubLog::createSubLog(sublog, createFilter("kmtvgame", true));
    p->wait();

    auto secondSub = any_cast<shared_ptr<SubLog>>(p->value());
    ASSERT_EQ(71204, secondSub->range().len());
}

TEST(SubLog, mapLine) {
    auto log = make_shared<FileLog>();
    log->open("sample.log");
    log->scheduleBuildBlocks()->wait();

    auto&& p = SubLog::createSubLog(log, createFilter("chromium", true));
    p->wait();

    auto firstSub = any_cast<shared_ptr<SubLog>>(p->value());
    ASSERT_EQ(19687, firstSub->mapToSource(3567));
    ASSERT_EQ(3567, firstSub->fromSource(19687));
    //firstSub中不存在19683源行，应找到其就近行
    ASSERT_EQ(3564, firstSub->fromSource(19683));

    p = SubLog::createSubLog(firstSub, createFilter("kmtvgame", true));
    p->wait();

    auto secondSub = any_cast<shared_ptr<SubLog>>(p->value());
    ASSERT_EQ(25512, secondSub->mapToSource(3567));
    ASSERT_EQ(3567, secondSub->fromSource(25512));
    //firstSub中不存在19683源行，应找到其就近行
    ASSERT_EQ(3560, secondSub->fromSource(25465));
    
}
TEST(SubLog, syncParentAppend) {
    auto loopThd = thread([]{EventLoop::instance().start();});

    auto log = make_shared<MonitorLog>();
#ifdef _WIN32
    string cmdLine = R"(Cscript /B 100-1-100.vbs)";
#else
    string cmdLine = "bash -c 'for i in {1..100}; do echo $i;done;sleep 1;for i in {1..100}; do echo $i;done;'";
#endif
    log->open(cmdLine,EventLoop::instance().base());

    this_thread::sleep_for(500ms);
    ASSERT_EQ(100, log->range().len());
    auto ret = SubLog::createSubLog(log, createFilter("0", true));
    ret->wait();
    auto sub = any_cast<shared_ptr<SubLog>>(ret->value());
    ASSERT_EQ(Range(0,9), sub->range());

    this_thread::sleep_for(1s);
    ASSERT_EQ(200, log->range().len());
    sub->syncParent();
    ASSERT_EQ(Range(0,19), sub->range());

    EventLoop::instance().stop();
    loopThd.join();
}

TEST(SubLog, syncSubSub) {//TODO:单独运行正常，连续运行异常
    auto loopThd = thread([]{EventLoop::instance().start();});

    auto log = make_shared<MonitorLog>();
#ifdef _WIN32
    string cmdLine = R"(Cscript /B 100-1-100.vbs)";
#else
    string cmdLine = "bash -c 'for i in {1..100}; do echo $i;done;sleep 1;for i in {1..100}; do echo $i;done;'";
#endif
    log->open(cmdLine,EventLoop::instance().base());

    this_thread::sleep_for(500ms);
    ASSERT_EQ(100, log->range().len());
    auto ret = SubLog::createSubLog(log, createFilter("0", true));
    ret->wait();
    auto sub = any_cast<shared_ptr<SubLog>>(ret->value());
    ASSERT_EQ(Range(0,9), sub->range());//10,20,30..90,100
    ret = SubLog::createSubLog(sub, createFilter("1", true));
    ret->wait();
    auto subsub = any_cast<shared_ptr<SubLog>>(ret->value());
    ASSERT_EQ(Range(0, 1), subsub->range());// 10,100

    this_thread::sleep_for(1s);
    ASSERT_EQ(200, log->range().len());
    sub->syncParent();
    ASSERT_EQ(Range(0,19), sub->range());
    subsub->syncParent();
    ASSERT_EQ(Range(0,3), subsub->range());//10,100,10,100

    EventLoop::instance().stop();
    loopThd.join();
}


TEST(SubLog, syncParentClip) {
    auto loopThd = thread([]{EventLoop::instance().start();});

    auto log = make_shared<MonitorLog>();
    log->setMaxBlockCount(2);

    //2个block最多存放65536行，当再推入100行的时候，第一个block被drop
#ifdef _WIN32
    string cmdLine = "Cscript /b 65536-5-10000.vbs";
#else
    string cmdLine = "bash -c 'for i in {1..65536}; do echo $i;done;sleep 5;for i in {1..10000}; do echo $i;done;'";
#endif
    log->open(cmdLine,EventLoop::instance().base());

    this_thread::sleep_for(4s);
    ASSERT_EQ(Range(0, 65535), log->range());
    auto ret = SubLog::createSubLog(log, createFilter(regex(R"(\d0000)")));
    ret->wait();
    auto sub = any_cast<shared_ptr<SubLog>>(ret->value());
    ASSERT_EQ(Range(0,5), sub->range());//10000、20000、30000、40000、50000、60000

    this_thread::sleep_for(5s);
    ASSERT_EQ(Range(32768, 75535), log->range());//第一个块已经被drop
    sub->syncParent();
    ASSERT_EQ(Range(3, 6), sub->range());//40000、50000、60000、70000

    EventLoop::instance().stop();
    loopThd.join();
}

TEST(SubLog, blankThenMatch) {
    auto loopThd = thread([]{EventLoop::instance().start();});

    auto log = make_shared<MonitorLog>();
#ifdef _WIN32
    string cmdLine = "echo 1 && timeout /t 1 && echo \"xyz\"";
#else
    string cmdLine = "echo 1;sleep 1;echo 'xyz'";
#endif
    log->open(cmdLine,EventLoop::instance().base());

    auto ret = SubLog::createSubLog(log, createFilter("xyz", true));
    ret->wait();
    auto sub = any_cast<shared_ptr<SubLog>>(ret->value());

    ASSERT_FALSE(sub->range().valid());

    this_thread::sleep_for(1.5s);
    sub->syncParent();

    ASSERT_EQ(Range(0,0), sub->range());
    ASSERT_EQ("xyz", string(sub->view(0,0)->current().str()));

    EventLoop::instance().stop();
    loopThd.join();
}

TEST(SubLog, blankThenMatch2) {
    auto loopThd = thread([]{EventLoop::instance().start();});

    auto log = make_shared<MonitorLog>();
#ifdef _WIN32
    string cmdLine = "echo 1 && timeout /t 1 && echo \"xyz\"";
#else
    string cmdLine = "echo 1;sleep 1;echo 'xyz'";
#endif
    log->open(cmdLine,EventLoop::instance().base());

    auto ret = SubLog::createSubLog(log, createFilter("xyz", true));
    ret->wait();
    auto sub = any_cast<shared_ptr<SubLog>>(ret->value());
    ret = SubLog::createSubLog(sub, createFilter("xyz", true));
    ret->wait();
    auto sub1 = any_cast<shared_ptr<SubLog>>(ret->value());

    ASSERT_FALSE(sub->range().valid());
    ASSERT_FALSE(sub1->range().valid());

    this_thread::sleep_for(1.5s);
    sub->syncParent();
    sub1->syncParent();

    ASSERT_EQ(Range(0,0), sub->range());
    ASSERT_EQ(Range(0,0), sub1->range());

    EventLoop::instance().stop();
    loopThd.join();
}