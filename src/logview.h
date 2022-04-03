#pragma once

#include <memory>
#include "def.h"
#include <string_view>

struct Block;
struct Line;
class Memory;

struct BlockRef {
    Block* block;
    Memory* mem;
};

struct LineRef {
    BlockRef refBlock;
    Line* line;
    BlockLineI indexInBlock;
    string_view str();
    LogLineI index();
};

class LogView {
public:
    virtual ~LogView() {}
    virtual LineRef current() const = 0;
    virtual void next() = 0;
    virtual bool end() = 0;
    virtual shared_ptr<LogView> subview(LogLineI from, LogLineI n) const = 0;
    virtual LogLineI size() const = 0;
};