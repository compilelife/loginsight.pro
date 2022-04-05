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
    
    mProcessExited = false;
    startListenFd(evbase);
    return true;
}

void MonitorLog::close() {
    event_del_block(mListenEvent);
    event_free(mListenEvent);

    closeProcess(mProcess);
    for (auto &&block : mBlocks)
        delete block;
    
    mBlocks.clear();

    mProcessExited = true;
}

void MonitorLog::setMaxBlockCount(size_t n) {
    mMaxBlockCount = n;
}

Range MonitorLog::range() const {
    if (mBlocks.empty())
        return Range();
    
    auto& firstBlock = mBlocks.front()->block;
    auto& lastBlock = mBlocks.back()->block;

    return {firstBlock.lineBegin, lastBlock.lineBegin + LastIndex(lastBlock.lines)};
}

shared_ptr<LogView> MonitorLog::view(LogLineI from, LogLineI to) const{
    vector<BlockRef> refs;
    for (auto &&b : mBlocks)
    {
        refs.push_back({&(b->block), &(b->mem)});
    }
    
    shared_ptr<LogView> view(new BlockLogView(move(refs)));

    if (from == 0 && to == InvalidLogLine)
        return view;

    return view->subview(from, to - from + 1);
}

void onPipeClosed(evutil_socket_t,short,void*) {
    LOGI("pipe closed");
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
    LogLineI lineBegin = 0;
    //检查最后一个block满了没
    if (!mBlocks.empty()) {
        auto& last = mBlocks.back();
        if (!last->isBackendFull())
            return last;
        lineBegin = last->block.lineBegin + last->block.lines.size();
    }

    //申请新的，要先看下第一块让不让移除
    if (mBlocks.size() >= mMaxBlockCount) {
        auto& first = mBlocks.front();
        if (!first->mem.requestAccess(first->mem.range(), Memory::Access::WRITE)) {
            return nullptr;
        }
        delete first;
        mBlocks.pop_front();
    }

    //申请一个新的
    auto ret = new MemBlock;
    ret->backend.resize(200 * 1024);
    ret->mem.reset(ret->backend.data(), {0, ret->backend.size() - 1});
    ret->block.lineBegin = lineBegin;

    mBlocks.push_back(ret);

    return ret;
}

bool MonitorLog::handlePendingTask(bool restartPending) {
    if (restartPending) {
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

    auto from = curBlock->lastFind;
    if (!from) from = pmem;

    string_view buf(from, curBlock->writePos - (from - pmem));
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
    curBlock->lastFind = last;
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
    int totalRead = 0;
    while (curPos < endPos) {
        auto howmuch = min(1024, (int)(endPos - curPos));
        auto n = readFd(mProcess.stdoutFd, pmem + curPos, howmuch);
        if (n <= 0)
            break;
        totalRead += n;
        curPos += n;
        if (n < howmuch)
            break;
    }

    //libevent告诉我们有数据可以读，但是read到0，说明进程已经退出了
    if (totalRead == 0) {
        event_del(mListenEvent);
        mProcessExited = true;
    }

    return totalRead > 0;
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

bool MonitorLog::canRemoveOldestBlock() {
    auto& first = *mBlocks.begin();

    auto fullRange = first->mem.range();
    auto access = Memory::Access::WRITE;
    auto canAccess = first->mem.requestAccess(fullRange, access);
    if (canAccess) {
        first->mem.unlock(fullRange, access);
    }
    return canAccess;
}