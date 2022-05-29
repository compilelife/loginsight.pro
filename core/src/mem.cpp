#include "mem.h"

char* Memory::reset(char* buf, Range range) {
    auto cache = mBuf;
    mBuf = buf;
    mRange = range;

    return cache;
}