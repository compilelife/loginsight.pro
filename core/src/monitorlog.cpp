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

MonitorLog::~MonitorLog() {
    close();
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
        evbuffer_enable_locking(mProcessBuf, nullptr);
        bool eof[] = {false};

        auto getCharThd = thread([this, &eof]{
            int r = 0;
            char ch;
            do {
                r = readProcess(mProcess, &ch, 1);
                // LOGI("%d, %c", r, ch);
                if (r > 0) {
                    evbuffer_add(mProcessBuf, &ch, 1);
                }
            }while(r > 0);
            eof[0] = true;
        });

        size_t lastLength = 0;
        while(!mClosed) {
            //下面一行会一直阻塞到数据足够才会返回；只能先退而求其次，用while read,后面需要改为异步IO
            // auto n = readProcess(mProcess, buf, 10240);
            //TODO
            //每隔10ms看数据是否增长，如没增长，或超过10K，则返回

            this_thread::sleep_for(30ms);

            auto curLength = evbuffer_get_length(mProcessBuf);
            if (curLength > 0 && (curLength == lastLength || curLength >= 10240 || eof[0])) {
                unique_lock<mutex> lk(mReadProcessMutex);

                auto p = EventLoop::instance().post(EventType::Write, [this]{
                    lock_guard<mutex> l(mReadProcessMutex);
                    handleReadStdOut();
                    mReadProcessCond.notify_all();
                    return Promise::resolved(true);
                });                

                mReadProcessCond.wait(lk);
                lastLength = evbuffer_get_length(mProcessBuf);
            } else {
                lastLength = curLength;
            }

            if (eof[0]) {
                activateAttr(LOG_ATTR_MAY_DISCONNECT, true);
                break;
            }
        }

        getCharThd.join();
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

void MonitorLog::clear() {
    for (auto &&block : mBlocks)
        delete block;
    
    mBlocks.clear();
    
    activateAttr(LOG_ATTR_DYNAMIC_RANGE, true);
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

            if (last < end && curBlock->block.lines.size() >= BLOCK_LINE_NUM) {//block里存放的行数超出了，开辟新的空间来存放
                mLastBlockTail = buf.substr(last - start);
                break;
            }
        }
    }
    curBlock->lastFind = SV_CPP20_ITER(last);
    
    if (!mLastBlockTail.empty())
        handleReadStdOut();
}

bool MonitorLog::readStdOutInto(MemBlock* curBlock) {
    //先消化上一个block的遗留
    auto& curPos = curBlock->writePos;//下一次循环可写位置，通过引用改写
    auto endPos = MORNITOR_BLOCK_SIZE;//backend结束位置，不可写的地方
    char* pmem = curBlock->backend.data();
    if (!mLastBlockTail.empty()) {
        memcpy(pmem + curPos, mLastBlockTail.data(), mLastBlockTail.length());
        curPos += mLastBlockTail.length();
        mLastBlockTail.clear();
    }

    auto howmuch = (size_t)(endPos - curPos);
    auto totalRead = evbuffer_remove(mProcessBuf, pmem+curPos, howmuch);
    // LOGI("%d, %s", totalRead, string(pmem+curPos, totalRead).c_str());
    if (totalRead > 0)
        curPos += totalRead;
    return totalRead > 0;
}

void MonitorLog::handleReadStdOut() {
    auto* curBlock = peekBlock();

    if (readStdOutInto(curBlock)) {
        splitLinesForNewContent(curBlock);
    }
}