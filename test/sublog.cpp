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

TEST(SubLog, mapLine) {
    FileLog log;
    log.open("./sample.log");
    log.scheduleBuildBlocks()->wait();

    auto&& p = SubLog::createSubLog(log.view(), createFilter("chromium", true));
    p->wait();

    auto firstSub = any_cast<shared_ptr<SubLog>>(p->value());
    ASSERT_EQ(19687, firstSub->mapToSource(3567));
    ASSERT_EQ(3567, firstSub->fromSource(19687));
    //firstSub中不存在19683源行，应找到其就近行
    ASSERT_EQ(3563, firstSub->fromSource(19683));

    p = SubLog::createSubLog(firstSub->view(), createFilter("kmtvgame", true));
    p->wait();

    auto secondSub = any_cast<shared_ptr<SubLog>>(p->value());
    ASSERT_EQ(25512, secondSub->mapToSource(3567));
    ASSERT_EQ(3567, secondSub->fromSource(25512));
    //firstSub中不存在19683源行，应找到其就近行
    ASSERT_EQ(3560, secondSub->fromSource(25465));
    
}