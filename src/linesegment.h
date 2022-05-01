#pragma once
#include <regex>
#include <map>
#include "def.h"
#include "log.h"

class LineSegment {
private:
    optional<regex> mRegex;
public:
    bool hasPatternSet();
    void setPattern(regex r);
    vector<Seg> formatLine(const string& line);
};