#include "mem.h"

char* Memory::reset(char* buf, Range range) {
    mReadRanges.clear();
    mWriteRanges.clear();
    auto cache = mBuf;
    mBuf = buf;
    mRange = range;

    return cache;
}

char* Memory::requestAccess(Range range, Access permission) {
    lock_guard<mutex> l(mAccessMutex);

    //检查是否可写
    auto rangeBeenWritten = find_if(mWriteRanges.begin(), 
                                    mWriteRanges.end(), 
                                    [&range](Range added){return added.overlapped(range);}
                                    ) != mWriteRanges.end();
    if (rangeBeenWritten)
        return nullptr;

    //读之间不需互斥，可以返回了
    if (permission == Access::READ) {
        mReadRanges.push_back(range);
        return mBuf+range.begin;
    }

    //继续判断是否被“读了”
    auto rangeBeenRead = find_if(mReadRanges.begin(), 
                                mReadRanges.end(), 
                                [&range](Range added){return added.overlapped(range);}
                                ) != mReadRanges.end();
    
    if (rangeBeenRead) 
        return nullptr;
    
    mWriteRanges.push_back(range);
    return mBuf + range.begin;
}

void Memory::unlock(Range range, Access permission) {
    lock_guard<mutex> l(mAccessMutex);

    auto& ranges = permission == Access::WRITE ? mWriteRanges : mReadRanges;

    auto target = find(ranges.begin(), ranges.end(), range);
    
    if (target != ranges.end())
        ranges.erase(target);
}