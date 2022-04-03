#include "monitorlog.h"
#include "blocklogview.h"
#include "stdout.h"
#include <algorithm>
#include "calculation.h"
#include <cstring>
#include "eventloop.h"

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

MonitorLog::MemBlock* MonitorLog::peekBlock() {
    //检查最后一个block满了没
    if (!mBlocks.empty()) {
        auto last = mBlocks.rbegin();
        if (!(*last)->isBackendFull())
            return (*last).get();
    }

    //申请新的，要先看下第一块让不让移除
    if (mBlocks.size() >= mMaxBlockCount) {
        auto& first = *(mBlocks.begin());
        if (!first->mem.requestAccess(first->mem.range(), Memory::Access::WRITE)) {
            return nullptr;
        }
        mBlocks.pop_front();
    }

    //申请一个新的
    unique_ptr<MemBlock> newBlock;
    newBlock->backend.resize(200 * 1024);
    newBlock->mem.reset(newBlock->backend.data(), {0, newBlock->backend.size() - 1});
    newBlock->block.lineBegin = range().end;

    auto ret = newBlock.get();
    mBlocks.push_back(move(newBlock));

    return ret;
}

bool MonitorLog::handlePendingTask(bool restartPending) {
    if (!restartPending) {
        if (!mPendingReadTask) {
            mPendingReadTask = PingTask::create(
                bind(&MonitorLog::canRemoveOldestBlock, this),
                [this]{this->handleReadStdOut(); return false;}
            );
            mPendingReadTask->start(EventLoop::instance().base(), 20);
        }
        return false;
    }
    
    //如果之前有pending的读操作，既然现在已经进来，那就可以释放掉了
    if (mPendingReadTask) {
        mPendingReadTask->stop();
        mPendingReadTask.reset();
    }

    return true;
}

void MonitorLog::splitLinesForNewContent(MemBlock* curBlock) {
    auto pmem = curBlock->backend.data();
    auto fromPos = curBlock->lastLineEndAt();
    string_view buf(pmem + fromPos, curBlock->writePos - fromPos);
    auto start = buf.begin();
    auto last = buf.begin();
    auto end = buf.end();
    while (last < end)
    {
        auto [newline, next] = findLine(last, end);
        if (newline == end) {
            if (curBlock->isBackendFull()) {
                //到end还未找到newline，且当前block的backend用完了，就需要放入下一个block
                mLastBlockTail = buf.substr(last - start);
            }
            break;
        } else {
            curBlock->block.lines.push_back({
                static_cast<BlockCharI>(last - start),
                static_cast<LineCharI>(newline - last)
            });
            last = next;
        }
    }
}

bool MonitorLog::readStdOutInto(MemBlock* curBlock) {
    //先消化上一个block的遗留
    auto& curPos = curBlock->writePos;//下一次循环可写位置
    auto endPos = curBlock->backend.capacity();//backend结束位置，不可写的地方
    char* pmem = curBlock->backend.data();
    if (!mLastBlockTail.empty()) {
        memcpy(pmem + curPos, mLastBlockTail.data(), mLastBlockTail.length());
        curPos += mLastBlockTail.length();
        mLastBlockTail.clear();
    }

    //如果一直将curBlock填充完还有数据，那就等下次回调再读
    while (curPos < endPos) {
        auto howmuch = min(1024, (int)(endPos - curPos));
        auto n = readFd(mProcess.stdoutFd, pmem + curPos, howmuch);
        if (n <= 0)
            break;
        curPos += n;
    }

    return true;
}

void MonitorLog::handleReadStdOut() {
    auto* curBlock = peekBlock();

    //curBlock == nullptr： 我们已经达到最大块数，且当前计算还未停止（memory被锁）
    if (!handlePendingTask(curBlock == nullptr))
        return;

    if (!readStdOutInto(curBlock))
        return;

    splitLinesForNewContent(curBlock);
}