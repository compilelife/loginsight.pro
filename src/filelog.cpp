#include "filelog.h"
#include <cstring>
#include <algorithm>
#include "blocklogview.h"
#include <filesystem>

bool FileLog::open(string_view path) {
    mPath = path;

    mMapInfo = createMapOfFile(path);
    if (!mMapInfo.valid())
        return false;

    mBuf.reset((char*)mMapInfo.addr, {0, mMapInfo.len - 1});

    mClosed = false;
    return true;
}

void FileLog::close() {
    if (mFileSizeWatcher) {
        mFileSizeWatcher->stop();
        mFileSizeWatcher.reset();
    }

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

    if (!mFileSizeWatcher)
        createFileSizeWatcher();
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

void FileLog::createFileSizeWatcher() {
    auto path = mPath;
    auto size = mMapInfo.len;

    mFileSizeWatcher = PingTask::create(
        [=]{return filesystem::file_size(path) > size;},
        [=] {
            //预期500ms一个文件的大小变化不会非常巨大，所以我们用单线程处理
            async(launch::async, &FileLog::onFileNewContent, this);
            //先停止watcher，避免反复触发
            return false;
        }
    );

    mFileSizeWatcher->start(500);
}

//在一个独立线程中执行
void FileLog::onFileNewContent() {
    auto oldSize = mMapInfo.len;
    auto mapInfo = createMapOfFile(mPath, oldSize);
    string_view viewOfNewContent((const char*)mapInfo.addr, mapInfo.len);
    
    bool cancel =false;
    auto blocks = any_cast<BlockChain>(buildBlock(&cancel, viewOfNewContent));

    //当内存可写时将block提交到filelog内
    PingTask::create(
        [this]{
            return this->mBuf.requestAccess(
                this->mBuf.range(),
                Memory::Access::WRITE
            );
        },
        [this, blocks] {
            //模拟关闭后重新打开（只是不清除之前的blocks信息）
            unmapFile(mMapInfo);
            if (!open(mPath)) {
                //FIXME:这里抛出的错误，并不会立即被检测到，在被检测到之前，如果有访问到内存的行为，将是不可预期的！！
                activateAttr(LOG_ATTR_MAY_DISCONNECT, true);
                return false;
            }
            //追加新的行信息
            copy(blocks.begin(), blocks.end(), back_inserter(mBlocks));
            mCount += blocks.size();
            //重新开始监听文件变化
            createFileSizeWatcher();
            return false;
        }
    )->start(20);
}