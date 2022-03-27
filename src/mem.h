#pragma once

#include <mutex>
#include "range.h"
#include <functional>
#include <vector>

using namespace std;

class Memory {
private:
    char* mBuf {nullptr};
    Range mRange;
    mutex mAccessMutex; //FIXME: 可能可以不需要这个锁，让主线程控制所有的内存访问
    vector<Range> mReadRanges;
    vector<Range> mWriteRanges;

public:
    enum Access {
        WRITE,
        READ,
    };

public:
    /**
     * @brief 由调用者确保在没有其他线程访问的情况reset
     */
    char* reset(char* buf, Range range);
    Range range() const {return mRange;}

public:
    char* requestAccess(Range range, Access permision);
    void unlock(Range range, Access permision);

public:
    char* unsafeAccess() const {
        return mBuf;
    }
};