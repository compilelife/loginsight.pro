#include "multifilelog.h"
#include "gtest/gtest.h"
#include <filesystem>

using namespace std::filesystem;

static vector<string_view> paths = {
        "multi/xaa", 
        "multi/xab", 
        "multi/xac", 
        "multi/xad", 
        "multi/xae", 
        "multi/xaf", 
        "multi/xag", 
        "multi/xah", 
        "multi/xai", 
        "multi/xaj", 
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
    fLog.open("sample.log");
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

TEST(MultiFileLog, listFilesByAlpha) {
    auto files = listFiles<false>("multi", regex("(.*)"));

    ASSERT_EQ(paths.size(), files.size());
    for (size_t i = 0; i < paths.size(); i++)
    {
        ASSERT_EQ(path(paths[i]), path(files[i]))<<i;
    }
}

TEST(MultiFileLog, listFilesByNum) {
    auto files = listFiles<true>("multi2", regex("(\\d+).log"));

    vector<string> expected{
        "multi2/1.log",
        "multi2/2.log",
        "multi2/3.log",
        "multi2/10.log",
        "multi2/11.log",
    };
    ASSERT_EQ(expected.size(), files.size());
    for (size_t i = 0; i < expected.size(); i++)
    {
        ASSERT_EQ(path(expected[i]), path(files[i]))<<i;
    }
}