#pragma once

#include <vector>
#include <array>
#include <optional>
#include <mutex>
#include "range.h"
#include "def.h"
#include "logview.h"
#include <memory>

/**
 * @brief 表示一行里的一个片段，用于行格式化结果的保存
 */
struct Seg {
    LineCharI offset;
    LineCharI length;
};

struct Line {
    /**
     * @brief 日志行在Block中的位置
     * 
     * 即，在Memory块的位置是：Block.fileOffset + Line.Offset
     */
    BlockCharI offset;
    LineCharI length;
    optional<vector<Seg>> segs;

    Line(BlockCharI o, LineCharI l) 
        :offset(o), length(l) {}
};

struct Block {
    LogLineI lineBegin{0};
    LogCharI offset{0};
    vector<Line> lines;
    Range bytesRange() {
        if (lines.empty())
            return Range();

        auto& lastLine = lines.back();
        return {
            offset,
            offset + lastLine.offset + lastLine.length - 1
        };
    }
};

class ILog {
public:
    virtual ~ILog() {}
    virtual shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const = 0;
    virtual Range range() const = 0;
};