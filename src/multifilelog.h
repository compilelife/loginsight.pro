#pragma once

#include "filelog.h"
#include <vector>

class MultiFileLog : public IClosableLog {
private:
    vector<unique_ptr<FileLog>> mLogs;
    LogLineI mCount{0};
public:
    shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const override;
    Range range() const override;

public:
    bool open(const vector<string_view>& paths);
    shared_ptr<Promise> scheduleBuildBlocks();
    void close() override;
};

vector<string> listFiles(string_view path, regex comparablePatten);