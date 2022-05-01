#include "filelog.h"
#include <cstring>
#include <algorithm>
#include "blocklogview.h"

bool FileLog::open(string_view path) {
    mMapInfo = createMapOfFile(path);
    if (!mMapInfo.valid())
        return false;

    mBuf.reset((char*)mMapInfo.addr, {0, mMapInfo.len - 1});

    mClosed = false;
    return true;
}

void FileLog::close() {
    unmapFile(mMapInfo);
    for (auto &&b : mBlocks)
    {
        delete b;
    }
    mBlocks.clear();

    mClosed = true;
}

shared_ptr<Promise> FileLog::scheduleBuildBlocks() {
    //确定块大小
    auto& calculation = Calculation::instance();

    auto len = mBuf.range().len();
    const char* mem = mBuf.requestAccess(mBuf.range(), Memory::Access::READ);
    auto memEnd = mem + len;

    const uint32_t leastLen = 1024*1024;

    auto step = len / calculation.coreNum();
    if (step < leastLen) {
        step = len >= leastLen ? leastLen : len;
    }

    //在换行的地方分割任务
    vector<string_view> tasks;

    while (memEnd - mem > 0) {
        auto taskMemEnd = mem + step;//预期结束位置
        string_view right{taskMemEnd, static_cast<size_t>(memEnd - taskMemEnd)};//预期位置后面的buf
        auto newLine = std::find(right.begin(), right.end(), '\n');//找到第一个换行
        //根据newLine切出合适的task
        if (newLine == right.end()) {
            tasks.push_back({mem, static_cast<size_t>(memEnd - mem)});
            break;
        } else {
            tasks.push_back({mem, static_cast<size_t>(newLine - mem + 1)});
            mem = newLine + 1;
        }
    }
    
    //提交任务
    auto ret = calculation.schedule(move(tasks), bind(&FileLog::buildBlock, this, placeholders::_1, placeholders::_2));
    ret->then([this](shared_ptr<Promise> p){
                    if (!p->isCancelled()) {
                        this->collectBlocks(p->calculationValue());
                    }
                    this->mBuf.unlock(this->mBuf.range(), Memory::Access::READ);
                });
    return ret;
}

any FileLog::buildBlock(bool* cancel, string_view buf) {
    auto begin = buf.begin();
    auto end = buf.end();

    auto veryBegin = (const char*)mMapInfo.addr;
    BlockChain blocks;

    auto createBlock = [veryBegin, &blocks](const char* from) -> Block* {
        auto b = new Block;
        b->offset = from - veryBegin;
        blocks.push_back(b);
        return blocks[blocks.size() - 1];
    };

    auto last = begin;
    BlockLineI lineIndex = BLOCK_LINE_NUM;
    Block* curBlock = nullptr;

    while (last < end && !*cancel)
    {
        if (lineIndex >= BLOCK_LINE_NUM) {
            lineIndex = 0;
            curBlock = createBlock(last);
        }

        auto [newline, next] = findLine(last, end);

        curBlock->lines.push_back({
            static_cast<BlockCharI>(last - (veryBegin + curBlock->offset)),
            static_cast<LineCharI>(newline - last)
        });

        last = next;
        ++lineIndex;
    }
    
    return blocks;
}

void FileLog::collectBlocks(const vector<any>& rets) {
    LogLineI count = 0;
    for (auto && ret : rets) {
        count += any_cast<BlockChain>(ret).size();
    }
    
    mBlocks.reserve(count);//一次性准备好内存
    auto pDest = mBlocks.data();
    
    for (auto && ret : rets) {
        auto blocks = any_cast<BlockChain>(ret);
        for (auto &&block : blocks)
            mBlocks.push_back(block);
    }

    count = 0;
    for (auto &&block : mBlocks) {
        block->lineBegin = count;
        count += block->lines.size();
    }
    
    mCount = count;
}

shared_ptr<LogView> FileLog::view(LogLineI from, LogLineI to) const {
    shared_ptr<LogView> view(new BlockLogView(mBlocks, &mBuf));
    if (from == 0 && to == InvalidLogLine)
        return view;
    
    return view->subview(from, to - from + 1);
}

Range FileLog::range() const {
    return mCount > 0 ? Range(0, mCount-1) : Range();
}