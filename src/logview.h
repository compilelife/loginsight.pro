#pragma once

#include <memory>
#include "def.h"

struct Block;
struct Line;
class Memory;

struct BlockRef {
    Block* block;
    Memory* mem;
};

struct LineRef {
    BlockRef bRef;
    Line* line;
};

class LogView {
public:
    virtual ~LogView() {}
    virtual LineRef current() const = 0;
    virtual void next() = 0;
    /**
     * @brief 划分到Block边界(所以实际>=n)
     * @param from 这里的line参数指的是在该logview里的第I行
     */
    virtual shared_ptr<LogView> subview(LogLineI from, LogLineI n) const = 0;
    virtual LogLineI size() const = 0;
};