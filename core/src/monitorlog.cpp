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
    
    mClosed = false;
    startListenFd(evbase);
    return true;
}

void MonitorLog::close() {
    if (mClosed)
        return;
        
    event_del_block(mListenEvent);
    event_free(mListenEvent);

    closeProcess(mProcess);
    for (auto &&block : mBlocks)
        delete block;
    
    mBlocks.clear();

    mClosed = true;
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
        if (!EventLoop::instance().canWrite()) {
            event_del(mListenEvent);//先取消监听，避免陷入一直被回调，一直post事件
            EventLoop::instance().post(EventType::Write, [this]{
                handleReadStdOut();
                event_add(mListenEvent, nullptr);//等处理完了再监听
                return Promise::resolved(true);
            });
            return nullptr;
        }
        delete mBlocks.front();
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
            curBlock->block.lines.push_back({
                static_cast<BlockCharI>(last - pmem),
                static_cast<LineCharI>(newline - last)
            });
            activateAttr(LOG_ATTR_DYNAMIC_RANGE, true);
            last = next;

            if (last < end && curBlock->block.lines.size() >= BLOCK_LINE_NUM) {//block里存放的行数超出了，开辟新的空间来存放；极低概率进这里，只有每行字符小于7个才有可能
                //FIXME:这是最简单的处理方法，即把剩下的数据等下次有新的输入时在处理，但会引入一些延迟
                mLastBlockTail = buf.substr(last - start);
                curBlock->lastFind = last;
                return;
            }
        }
    }
    curBlock->lastFind = last;
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
        activateAttr(LOG_ATTR_MAY_DISCONNECT, true);
    }

    return totalRead > 0;
}

void MonitorLog::handleReadStdOut() {
    auto* curBlock = peekBlock();

    if (curBlock == nullptr)
        return;

    if (!readStdOutInto(curBlock))
        return;

    splitLinesForNewContent(curBlock);
}