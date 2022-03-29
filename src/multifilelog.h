#pragma once

#include "filelog.h"

class MultiFileLog : public ILog {
private:
    vector<unique_ptr<FileLog>> mLogs;
public:
    shared_ptr<LogView> view() const override;
    Range range() const override;

public:
    bool open(const vector<string_view>& path);
    //1. future + bool*
    //2. Calculation = MyPromise + schedule; returns MyPromise instead
    Calculation& scheduleBuildBlocks();
    void close();
};