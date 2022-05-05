#include "filelog.h"
#include "gtest/gtest.h"
#include <filesystem>
#include <vector>
#include "stdout.h"
#include "eventloop.h"
#include <fstream>

using namespace std;

#define TestBuildBlock(str, expectedLineNums) \
{bool cancel = false;\
auto blocks = any_cast<vector<Block*>>(log.buildBlock(&cancel, str));\
vector<int> expected = expectedLineNums;\
ASSERT_EQ(expected.size(), blocks.size());\
for (size_t i = 0; i < expected.size(); i++)\
    ASSERT_EQ(expected[i], blocks[i]->lines.size());\
}


TEST(FileLog, buildBlock) {
    FileLog log;
    ASSERT_TRUE(log.open("./sample.log"));

    auto base = static_cast<const char*>(log.mMapInfo.addr);
    
    TestBuildBlock(string_view(base, 549), {2});
    TestBuildBlock(string_view(base, 550), {2});
    TestBuildBlock(string_view(base, 551), {3});
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
    ASSERT_TRUE(log.open("./empty.log"));

    log.scheduleBuildBlocks()->wait();

    auto range = log.range();
    ASSERT_FALSE(range.valid());

    log.close();
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
    auto view = log.view();
    ASSERT_EQ("1", view->current().str());
    view->next();
    ASSERT_EQ("2", view->current().str());

    //view只读占用内存时，文件变化Pending，因此行数仍然是2
    addLine(logpath, "3");
    this_thread::sleep_for(1s);
    ASSERT_EQ(2, log.range().len());

    //一旦view释放内存，则立即检测到变化
    view.reset();
    this_thread::sleep_for(100ms);
    ASSERT_EQ(3, log.range().len());

    EventLoop::instance().stop();
    loopThd.join();
}