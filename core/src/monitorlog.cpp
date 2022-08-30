#include "monitorlog.h"
#include "blocklogview.h"
#include "stdout.h"
#include <algorithm>
#include "calculation.h"
#include <cstring>
#include "eventloop.h"

bool MonitorLog::open(string_view cmdline, event_base* evbase) {
    mProcess = openProcess(cmdline);
    if (!mProcess)
        return false;
    
    mClosed = false;
    startListenFd();
    return true;
}

void MonitorLog::close() {
    if (mClosed)
        return;

    mClosed = true;

    closeProcess(mProcess);
    mReadProcessCond.notify_all();
    mReadProcessThd.join();
    
    for (auto &&block : mBlocks)
        delete block;
    
    mBlocks.clear();

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

    auto curRange = range();
    from -= curRange.begin;
    to -= curRange.begin;

    return view->subview(from, to - from + 1);
}

void MonitorLog::startListenFd() {
    mReadProcessThd = thread([this]{
        mProcessBuf = evbuffer_new();

        while(!mClosed) {
            char buf[10240] = {0};
            auto n = readProcess(mProcess, buf, 10240);
            if (n > 0) {
                evbuffer_add(mProcessBuf, buf, n);
                EventLoop::instance().post(EventType::Write, [this]{
                    handleReadStdOut();
                    return Promise::resolved(true);
                });
            } else {
                activateAttr(LOG_ATTR_MAY_DISCONNECT, true);
                break;
            }

            {//等待“主线程”处理完成
                unique_lock<mutex> lk(mReadProcessMutex);
                mReadProcessCond.wait(lk);
            }
        }

        evbuffer_free(mProcessBuf);
    });
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

    //申请新的块
    if (mBlocks.size() >= mMaxBlockCount) {
        delete mBlocks.front();
        mBlocks.pop_front();
    }

    //申请一个新的
    auto ret = new MemBlock;
    ret->backend.resize(MORNITOR_BLOCK_SIZE);
    ret->mem.reset(ret->backend.data(), {0, ret->backend.size() - 1});
    ret->block.lineBegin = lineBegin;

    mBlocks.push_back(ret);

    return ret;
}

void MonitorLog::splitLinesForNewContent(MemBlock* curBlock) {
    auto pmem = curBlock->backend.data();

    auto from = curBlock->lastFind;
    if (!from) from = pmem;

    //----------------------
    //^        ^ ------buf----^
    //|pmem    |from          |writePos
    string_view buf(from, curBlock->writePos - (from - pmem));
    auto start = buf.begin();
    auto last = buf.begin();
    auto end = buf.end();
    while (last < end)
    {
        auto [newline, next] = findNewLine(last, end);
        if (newline == end) {
            if (curBlock->isBackendFull()) {
                //到end还未找到newline，且当前block的backend用完了，就需要放入下一个block
                mLastBlockTail = buf.substr(last - start);
            }
            break;
        } else {
            auto offset = (int)(SV_CPP20_ITER(last) - pmem);
            auto len = (int)(newline - last);
            curBlock->block.lines.push_back({
                static_cast<BlockCharI>(offset),
                static_cast<LineCharI>(len)
            });
            activateAttr(LOG_ATTR_DYNAMIC_RANGE, true);
            last = next;

            if (last < end && curBlock->block.lines.size() >= BLOCK_LINE_NUM) {//block里存放的行数超出了，开辟新的空间来存放；极低概率进这里，只有每行字符小于7个才有可能
                //FIXME:这是最简单的处理方法，即把剩下的数据等下次有新的输入时在处理，但会引入一些延迟
                mLastBlockTail = buf.substr(last - start);
                curBlock->lastFind = SV_CPP20_ITER(last);
                return;
            }
        }
    }
    curBlock->lastFind = SV_CPP20_ITER(last);
}

bool MonitorLog::readStdOutInto(MemBlock* curBlock) {
    //先消化上一个block的遗留
    auto& curPos = curBlock->writePos;//下一次循环可写位置，通过引用改写
    auto endPos = curBlock->backend.capacity();//backend结束位置，不可写的地方
    char* pmem = curBlock->backend.data();
    if (!mLastBlockTail.empty()) {
        memcpy(pmem + curPos, mLastBlockTail.data(), mLastBlockTail.length());
        curPos += mLastBlockTail.length();
        mLastBlockTail.clear();
    }

    auto howmuch = (size_t)(endPos - curPos);
    auto totalRead = evbuffer_remove(mProcessBuf, pmem+curPos, howmuch);
    if (totalRead > 0)
        curPos += totalRead;
    
    return totalRead > 0;
}

void MonitorLog::handleReadStdOut() {
    auto* curBlock = peekBlock();

    if (readStdOutInto(curBlock)) {
        splitLinesForNewContent(curBlock);
    }

    mReadProcessCond.notify_all();
}