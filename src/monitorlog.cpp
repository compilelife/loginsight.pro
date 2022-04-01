#include "monitorlog.h"
#include "blocklogview.h"
#include "stdout.h"

bool MonitorLog::open(string_view cmdline, event_base* evbase) {
    mProcess = openProcess(cmdline);
    if (mProcess.stdoutFd < 0)
        return false;
    
    startListenFd(evbase);
    return true;
}

void MonitorLog::close() {
    event_del_block(mListenEvent);
    event_free(mListenEvent);

    closeProcess(mProcess);
    mBlocks.clear();
}

void MonitorLog::setMaxBlockCount(size_t n) {
    mMaxBlockCount = n;
}

Range MonitorLog::range() const {
    if (mBlocks.empty())
        return Range();
    
    auto& firstBlock = (*mBlocks.begin())->block;
    auto& lastBlock = (*mBlocks.rbegin())->block;

    return {firstBlock.lineBegin, lastBlock.lineBegin + LastIndex(lastBlock.lines)};
}

shared_ptr<LogView> MonitorLog::view(LogLineI from, LogLineI to) const{
    vector<BlockRef> refs;
    for (auto &&b : mBlocks)
    {
        refs.push_back({&(b->block), &(b->mem)});
    }
    
    shared_ptr<LogView> view(new BlockLogView(move(refs)));

    if (Range(from, to) == range()) {
        return view;
    }

    return view->subview(from, to - from + 1);
}

void MonitorLog::startListenFd(event_base* evbase) {
    mListenEvent = event_new(evbase, mProcess.stdoutFd, EV_PERSIST|EV_READ,
    [](evutil_socket_t fd, short flags, void *arg){
        auto thiz = static_cast<MonitorLog*>(arg);
        thiz->handleReadStdOut();
    }, this);
    event_add(mListenEvent, nullptr);
}

void MonitorLog::handleReadStdOut() {
    auto& curBlock = peekBlock();
    //check access;
    //if no access...make period check
    //将周期性检查任务抽象出复用
    //else write to curBlock, do split line and formatting
    //将行分割算法和行格式化算法单独抽出来复用
}