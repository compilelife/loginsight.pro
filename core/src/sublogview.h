#pragma once

#include "logview.h"

class SubLog;

struct SubLogPos
{
    size_t blockIndex;
    size_t lineIndex;
};

class SubLogView : public LogView {
private:
    SubLog* mLog;
    SubLogPos mFrom;
    SubLogPos mTo;
    SubLogPos mCur;
    LogLineI mCount;
    bool mReverse{false};

public:
    SubLogView(const SubLog* log);
    SubLogView(SubLog* log, SubLogPos from, SubLogPos to, LogLineI n);
    ~SubLogView();

private:
    void lockMemory();

public:
    LineRef current() const override;
    void next() override;
    bool end() override;
    shared_ptr<LogView> subview(LogLineI from, LogLineI n) const override;
    LogLineI size() const override;
    void reverse() override;

private:
    SubLogPos locateLine(LogLineI line) const;
};