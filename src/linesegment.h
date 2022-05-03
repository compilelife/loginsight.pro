#pragma once
#include <regex>
#include <map>
#include <any>
#include "def.h"
#include "log.h"

enum SegType {
    Date,
    LogLevel,
    Num,
    Str
};

enum LogLevel {
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

struct Segment {
    SegType type;
    string name;
    /**
     * Date: 用于strftime的格式化字符串
     * LogLevel: 用于将各式各样的等级字符串映射为LogLevel枚举值
     */
    any extra;
};

class LineSegment {
private:
    optional<regex> mRegex;
    vector<Segment> mSegments;
public:
    bool hasPatternSet();
    void setPattern(regex r);
    void setSegments(vector<Segment>&& segmentsInGroupOrder);
    vector<LineSeg> formatLine(const string& line);
public:
    string makeDebugPrint(string_view line, const vector<LineSeg>& segs);
};