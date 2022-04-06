#include "mem.h"
#include "gtest/gtest.h"
#include "filelog.h"

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

    
}