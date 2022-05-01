#pragma once

#include "filelog.h"
#include <vector>

class MultiFileLog : public SourceLog {
private:
    vector<unique_ptr<FileLog>> mLogs;
    LogLineI mCount{0};
public:
    MultiFileLog() {mAttrs = LOG_ATTR_DYNAMIC_RANGE;}
    shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const override;
    Range range() const override;

public:
    bool open(const vector<string_view>& paths);
    shared_ptr<Promise> scheduleBuildBlocks();
    void close() override;
};

/**
 * @brief 对path递归遍历，使用comparablePattern进行检索和排序
 * 
 * @param path 多份日志存放的位置
 * @param comparablePattern 匹配文件名，并至少有一个group，返回可比较的内容
 * @return vector<string> 排序好的所有日志文件
 */
template<bool IsCompareNum>
vector<string> listFiles(string_view path, regex comparablePattern);