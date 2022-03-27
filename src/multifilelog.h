#pragma once

#include "def.h"
#include "log.h"
#include "calculation.h"

class MultiFileLog : public ILog {
private:

public:
    unique_ptr<LogView> view() const override;
    Range range() const override;

public:
    bool open(const vector<string_view>& path);
    Calculation& scheduleBuildBlocks();
    void close();
};