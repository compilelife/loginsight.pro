#include "filelog.h"
#include "gtest/gtest.h"
#include <filesystem>
#include <vector>
#include "stdout.h"

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