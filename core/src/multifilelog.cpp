#include "multifilelog.h"
#include <algorithm>
#include "blocklogview.h"
#include <filesystem>
#include <queue>

bool MultiFileLog::open(const vector<string>& paths) {
    mLogs.clear();

    for (auto &&path : paths)
    {
        auto log = unique_ptr<FileLog>(new FileLog);
        if (!log->open(path))
            return false;
        
        mLogs.push_back(move(log));
    }

    mClosed = false;
    return true;    
}

shared_ptr<Promise> MultiFileLog::scheduleBuildBlocks() {
    vector<shared_ptr<Promise>> logBuilds;
    for (auto &&log : this->mLogs)
        logBuilds.push_back(log->scheduleBuildBlocks());
    
    auto ret = Promise::all(move(logBuilds));
    ret->then([this](shared_ptr<Promise> p){
        if (p->isCancelled())
            return;
            
        for (auto &&log: this->mLogs)
            this->mCount += log->range().len();
    }, false);

    return ret;
}

void MultiFileLog::close() {
    for (auto &&log : mLogs)
        log->close();
    mClosed = true;
}

Range MultiFileLog::range() const {
    return mCount > 0 ? Range{0, mCount - 1} : Range();
}

shared_ptr<LogView> MultiFileLog::view(LogLineI from, LogLineI to) const {
    vector<BlockRef> allRefs;
    for (auto &&log : mLogs)
        for (auto &&block : log->mBlocks)
            allRefs.push_back({block, &(log->mBuf)});

    shared_ptr<LogView> view(new BlockLogView(move(allRefs)));

    if (from == 0 && to == InvalidLogLine)
        return view;

    return view->subview(from, to - from + 1);
}

template<class ComparePartType>
struct ListFileQueue {
    struct Item
    {
        ComparePartType comparePart;
        string path;
        bool operator < (const Item& i) const{
            return comparePart > i.comparePart;
        }
    };
    
    priority_queue<Item> q;
};

template<bool IsCompareNum>
struct ListFileHelper{};

template<>
struct ListFileHelper<true> {
    using PriorityQueueType = ListFileQueue<int>;
    static PriorityQueueType::Item createItem(string comparePart, string path) {
        return {stoi(comparePart), path};
    }
};