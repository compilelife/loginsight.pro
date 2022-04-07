#include "sublog.h"
#include "gtest/gtest.h"
#include "def.h"
#include "filelog.h"

TEST(SubLog, create) {
    FileLog log;
    log.open("./sample.log");
    log.scheduleBuildBlocks()->wait();

    auto&& p = SubLog::createSubLog(log.view(), createFilter("chromium", true));
    p->wait();

    auto sublog = any_cast<shared_ptr<SubLog>>(p->value());

    ASSERT_EQ(132852, sublog->range().len());
}

TEST(SubLog, subLogOfSubLog) {
    FileLog log;
    log.open("./sample.log");
    log.scheduleBuildBlocks()->wait();

    auto&& p = SubLog::createSubLog(log.view(), createFilter("chromium", true));
    p->wait();

    auto sublog = any_cast<shared_ptr<SubLog>>(p->value());

    p = SubLog::createSubLog(sublog->view(), createFilter("kmtvgame", true));
    p->wait();

    auto secondSub = any_cast<shared_ptr<SubLog>>(p->value());
    ASSERT_EQ(71204, secondSub->range().len());
}