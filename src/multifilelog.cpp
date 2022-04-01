#include "multifilelog.h"
#include <algorithm>
#include "blocklogview.h"

bool MultiFileLog::open(const vector<string_view>& paths) {
    mLogs.clear();

    for (auto &&path : paths)
    {
        auto log = unique_ptr<FileLog>(new FileLog);
        if (!log->open(path))
            return false;
        
        mLogs.push_back(move(log));
    }

    return true;    
}

unique_ptr<Promise> MultiFileLog::scheduleBuildBlocks() {
    return unique_ptr<Promise>(new Promise([this](bool* cancel){
        vector<unique_ptr<Promise>> logBuilds;
        for (auto &&log : this->mLogs)
            logBuilds.push_back(log->scheduleBuildBlocks());
        
        bool allDone = false;
        do {
            allDone = all_of(logBuilds.begin(), logBuilds.end(), 
                [](unique_ptr<Promise>& build){
                    return build->wait(10);
                });
        } while (!allDone && !*cancel);
        
        if (*cancel) {
            for (auto &&build: logBuilds)
                build->cancel();
        } else {
            for (auto &&log: this->mLogs)
                this->mCount += log->range().len();
        }

        return *cancel;
    }));
}

void MultiFileLog::close() {
    for (auto &&log : mLogs)
        log->close();
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