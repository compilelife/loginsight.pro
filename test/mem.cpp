#include "mem.h"
#include "gtest/gtest.h"
#include "filelog.h"
#include "sublog.h"

TEST(Memory, ReadNotExcludeRead) {
    Memory mem;
    char buffer[100];
    mem.reset(buffer, {0,99});

    ASSERT_NE(nullptr, mem.requestAccess({0,20}, Memory::Access::READ));
    ASSERT_NE(nullptr, mem.requestAccess({10,30}, Memory::Access::READ));
}

TEST(Memory, WriteExcludeRead) {
    Memory mem;
    char buffer[100];
    mem.reset(buffer, {0,99});

    ASSERT_NE(nullptr, mem.requestAccess({0,20}, Memory::Access::WRITE));
    ASSERT_EQ(nullptr, mem.requestAccess({10,30}, Memory::Access::READ));
}

TEST(Memory, WriteExcludeWrite) {
    Memory mem;
    char buffer[100];
    mem.reset(buffer, {0,99});

    ASSERT_NE(nullptr, mem.requestAccess({0,20}, Memory::Access::WRITE));
    ASSERT_EQ(nullptr, mem.requestAccess({10,30}, Memory::Access::WRITE));
}

TEST(Memory, Unlock) {
    Memory mem;
    char buffer[100];
    mem.reset(buffer, {0,99});

    ASSERT_NE(nullptr, mem.requestAccess({0,20}, Memory::Access::WRITE));
    mem.unlock({0, 20}, Memory::Access::WRITE);
    ASSERT_NE(nullptr, mem.requestAccess({10,30}, Memory::Access::WRITE));
}

TEST(Memory, LockWhenView) {
    FileLog log;
    log.open("./204800.log");
    log.scheduleBuildBlocks()->wait();

    auto assertCanAccess = [&log] (string_view desc) {
        ASSERT_NE(nullptr, log.mBuf.requestAccess(log.range(), Memory::Access::WRITE))<<desc;
        log.mBuf.unlock(log.range(), Memory::Access::WRITE);
    };
    auto assertNoAccess = [&log] (string_view desc)  {
        ASSERT_EQ(nullptr, log.mBuf.requestAccess(log.range(), Memory::Access::WRITE))<<desc;
    };

    assertCanAccess("no view: can access");

    {
        auto logview = log.view();
        assertNoAccess("with view: no access");
    }

    assertCanAccess("release view: can access");

    {
        auto p = SubLog::createSubLog(log.view(), createFilter("chromium", true));
        p->wait();
        auto sub = any_cast<shared_ptr<SubLog>>(p->value());

        assertCanAccess("sub created: can access");

        auto subview = sub->view();

        assertNoAccess("sub view: no access");//sub view的时候，根log也不允许访问
    }

    assertCanAccess("release sub view: can access");
}