#include "filelog.h"
#include "gtest/gtest.h"
#include <filesystem>
#include <vector>
#include "stdout.h"
#include "eventloop.h"
#include <fstream>
#include "stdout.h"

using namespace std;

#define TestBuildBlock(str, expectedLineNums, base) \
{bool cancel = false;\
auto blocks = any_cast<vector<Block*>>(log.buildBlock(&cancel, str, base));\
vector<int> expected = expectedLineNums;\
ASSERT_EQ(expected.size(), blocks.size());\
for (size_t i = 0; i < expected.size(); i++)\
    ASSERT_EQ(expected[i], blocks[i]->lines.size());\
}


TEST(FileLog, buildBlock) {
    FileLog log;
    ASSERT_TRUE(log.open("./sample.log"));

    auto base = static_cast<const char*>(log.mMapInfo.addr);
    
    TestBuildBlock(string_view(base, 549), {2}, base);
    TestBuildBlock(string_view(base, 550), {2}, base);
    TestBuildBlock(string_view(base, 551), {3}, base);
}

TEST(FileLog, open) {
    FileLog log;
    ASSERT_TRUE(log.open("./sample.log"));

    log.scheduleBuildBlocks()->wait();

    ASSERT_EQ(534075, log.range().len());

    log.close();
}

TEST(FileLog, openEmptyLog) {
    FileLog log;
    ASSERT_FALSE(log.open("./empty.log"));
}

TEST(FileLog, openNewLine) {
    FileLog log;
    ASSERT_TRUE(log.open("./newline.log"));

    log.scheduleBuildBlocks()->wait();

    ASSERT_EQ(1, log.range().len());

    log.close();
}

TEST(FileLog, mapLine) {
    FileLog log;
    ASSERT_TRUE(log.open("./sample.log"));

    log.scheduleBuildBlocks()->wait();

    ASSERT_EQ(3000, log.mapToSource(3000));
    ASSERT_EQ(3000, log.fromSource(3000));
}

static void addLine(filesystem::path p, string_view content) {
    ofstream s(p, ios_base::app);
    s<<content<<"\n";
    s.close();
}

TEST(FileLog, fileChanged) {
    auto logpath = filesystem::temp_directory_path()/"changed.log";
    filesystem::remove(logpath);

    addLine(logpath, "1");//新增一行让文件存在

    thread loopThd([]{EventLoop::instance().start();});

    FileLog log;
    ASSERT_TRUE(log.open(logpath.c_str()));
    log.scheduleBuildBlocks()->wait();
    ASSERT_EQ(1, log.range().len());

    //内部500ms检测文件变化，预期1s后肯定能发现行数为2
    addLine(logpath, "2");
    this_thread::sleep_for(1s);
    ASSERT_EQ(2, log.range().len());
    ASSERT_TRUE(log.isAttrAtivated(LOG_ATTR_DYNAMIC_RANGE));

    //检查内容
    LOGI("post begin");
    EventLoop::instance().post(EventType::Read, [&log]{
        auto checkView = [&log]{
            auto view = log.view();
            ASSERT_EQ("1", view->current().str());//返回void,所以放lambda里
            view->next();
            ASSERT_EQ("2", view->current().str());
        };
        checkView();
        this_thread::sleep_for(2s);
        return Promise::resolved(true);
    });
LOGI("post end");
    //有read事件时，文件变化Pending，因此行数仍然是2
    addLine(logpath, "3");
    this_thread::sleep_for(1s);
    ASSERT_EQ(2, log.range().len());

    //read事件结束，则可以检测到变化
    this_thread::sleep_for(2s);
    ASSERT_EQ(3, log.range().len());

    EventLoop::instance().stop();
    loopThd.join();
}