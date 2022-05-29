#pragma once

#include "def.h"
#include "log.h"
#include "mem.h"
#include "platform.h"
#include "calculation.h"
#include "pingtask.h"

class FileLog: public SourceLog {
private:
    friend class MultiFileLog;
    using BlockChain = vector<Block*>;
    Memory mBuf;
    MMapInfo mMapInfo;
    vector<Block*> mBlocks;
    LogLineI mCount;
    //检查文件变化；
    //然后在其他线程计算行信息（只map新增部分），计算完成后，
    //创建另一个临时Pingtask，在无memory lock时，在主线程修改mBlocks/mmap信息
    shared_ptr<PingTask> mFileSizeWatcher;
    string mPath;

public:
    FileLog() {mAttrs = LOG_ATTR_DYNAMIC_RANGE | LOG_ATTR_MAY_DISCONNECT;}
    ~FileLog();
    shared_ptr<LogView> view(LogLineI from = 0, LogLineI to = InvalidLogLine) const override;
    Range range() const override;

public:
    bool open(string_view path);
    shared_ptr<Promise> scheduleBuildBlocks();
    void close() override;

private:
    friend class FileLog_buildBlock_Test;
    friend class Memory_LockWhenView_Test;
    static any buildBlock(bool* cancel, string_view buf, const char* veryBegin);
    void collectBlocks(const vector<any>& rets);

    void createFileSizeWatcher();
    void onFileNewContent();
};