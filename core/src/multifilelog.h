#pragma once

#include "filelog.h"
#include <vector>

class MultiFileLog : public SourceLog {
private:
    vector<unique_ptr<FileLog>> mLogs;
    LogLineI mCount{0};
public:
    MultiFileLog() {mAttrs = 0;}
    shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const override;
    Range range() const override;

public:
    bool open(const vector<string>& paths);
    shared_ptr<Promise> scheduleBuildBlocks();
    void close() override;
};