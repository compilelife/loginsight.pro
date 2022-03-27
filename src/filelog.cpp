#include "filelog.h"
#include <cstring>
#include <algorithm>
#include "blocklogview.h"

bool FileLog::open(string_view path) {
    mMapInfo = createMapOfFile(path);
    if (!mMapInfo.valid())
        return false;

    mBuf.reset((char*)mMapInfo.addr, {0, mMapInfo.len - 1});

    return true;
}

void FileLog::close() {
    unmapFile(mMapInfo);
    for (auto &&b : mBlocks)
    {
        delete b;
    }
    mBlocks.clear();
}

Calculation& FileLog::scheduleBuildBlocks() {
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
        string_view right{taskMemEnd, static_cast<size_t>(memEnd - mem)};//预期位置后面的buf
        auto newLine = find(right.begin(), right.end(), '\n');//找到第一个换行
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
    return calculation.schedule(move(tasks), 
                         bind(&FileLog::buildBlock, this, placeholders::_1))
                .then(bind(&FileLog::collectBlocks, this, placeholders::_1))
                .finally([this]{
                    this->mBuf.unlock(this->mBuf.range(), Memory::Access::READ);
                });
}

any FileLog::buildBlock(string_view buf) {
    auto begin = buf.begin();
    auto end = buf.end();

    auto veryBegin = (const char*)mMapInfo.addr;
    BlockChain blocks;
    blocks.reserve(min(buf.size() / 100, 1uL));//预估的行数，避免频繁扩展blocks内存

    auto createBlock = [veryBegin, &blocks](const char* from) -> Block* {
        auto b = new Block;
        b->offset = from - veryBegin;
        blocks.push_back(b);
        return blocks[blocks.size() - 1];
    };

    auto last = begin;
    BlockLineI lineIndex = BLOCK_LINE_NUM;
    Block* curBlock = nullptr;

    while (last < end && !Calculation::instance().isCancelled())
    {
        if (lineIndex >= BLOCK_LINE_NUM) {
            lineIndex = 0;
            curBlock = createBlock(last);
        }

        auto it = find(last, end, '\n');

        curBlock->lines.push_back({
            static_cast<BlockCharI>(last - (veryBegin + curBlock->offset)),
            static_cast<LineCharI>(it-last)
        });

        last = it + 1;
        ++lineIndex;
    }
    
    return blocks;
}

void FileLog::collectBlocks(vector<any>& rets) {
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

unique_ptr<LogView> FileLog::view() const {
    auto view = new BlockLogView(mBlocks, &mBuf);
    return unique_ptr<LogView>();
}

Range FileLog::range() const {
    return mCount > 0 ? Range(0, mCount-1) : Range();
}