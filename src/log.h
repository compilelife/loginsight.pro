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
struct LineSeg {
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
    optional<vector<LineSeg>> segs;

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

#define LOG_ATTR_DYNAMIC_RANGE 0x01
#define LOG_ATTR_MAY_DISCONNECT 0x02

class ILog {
protected:
    size_t mAttrs{0};
    size_t mAttrAtivated{0};
public:
    bool hasAttr(size_t attr) {
        return (mAttrs & attr) == attr;
    }
    bool isAttrAtivated(size_t attr)  {
        return (mAttrAtivated & attr) > 0;
    }
    void activateAttr(size_t attr, bool v) {
        if (v) mAttrAtivated |= attr;
        else mAttrAtivated &= ~attr;
    }
public:
    virtual ~ILog() {}
    virtual shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const = 0;
    virtual Range range() const = 0;
    virtual LogLineI mapToSource(LogLineI index) const = 0;
    virtual LogLineI fromSource(LogLineI index) const = 0;
};

class IClosable {
protected:
    bool mClosed{true};
public:
    virtual ~IClosable(){}
    virtual void close() = 0;
    bool isClosed() const {return mClosed;}
};

class IClosableLog: public ILog, public IClosable {

};

class SourceLog: public IClosableLog {
public:
    LogLineI mapToSource(LogLineI index) const override {return index;}
    LogLineI fromSource(LogLineI index) const override {return index;}
};