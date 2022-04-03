#include "multifilelog.h"
#include "gtest/gtest.h"

static vector<string_view> paths = {
        "./multi/xaa", 
        "./multi/xab", 
        "./multi/xac", 
        "./multi/xad", 
        "./multi/xae", 
        "./multi/xaf", 
        "./multi/xag", 
        "./multi/xah", 
        "./multi/xai", 
        "./multi/xaj", 
};

TEST(MultiFileLog, openMulti) {
    MultiFileLog log;
    ASSERT_TRUE(log.open(paths));

    auto p = log.scheduleBuildBlocks();
    ASSERT_TRUE(p->wait());

    ASSERT_EQ(534075, log.range().len());

    log.close();
}