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

TEST(MultiFileLog, content) {
    MultiFileLog log;
    log.open(paths);
    log.scheduleBuildBlocks()->wait();
    auto multiView = log.view();

    FileLog fLog;
    fLog.open("./sample.log");
    fLog.scheduleBuildBlocks()->wait();
    auto singleView = fLog.view();

    ASSERT_EQ(multiView->size(), singleView->size());

    while (!multiView->end() && !singleView->end())
    {
        ASSERT_TRUE(multiView->current().str() == singleView->current().str());
        multiView->next();
        singleView->next();
    }
}