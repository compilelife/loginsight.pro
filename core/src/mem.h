#pragma once

#include <mutex>
#include "range.h"
#include <functional>
#include <vector>

using namespace std;

//之前想着用内存管理读写权限，后来通过事件读写属性更好控制，所以现在Memory只是一个壳了
class Memory {
private:
    char* mBuf {nullptr};
    Range mRange;

public:
    char* reset(char* buf, Range range);
    Range range() const {return mRange;}
    char* access() const {
        return mBuf;
    }
};