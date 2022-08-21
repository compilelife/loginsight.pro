#include "filelog.h"
#include <cstring>
#include <algorithm>
#include "blocklogview.h"
#include <filesystem>
#include "eventloop.h"
#include "stdout.h"

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
    if (mClosed)
        return;

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

FileLog::~FileLog() {
    close();
}

shared_ptr<Promise> FileLog::scheduleBuildBlocks() {
    //确定块大小
    auto& calculation = Calculation::instance();

    auto len = mBuf.range().len();
    const char* mem = mBuf.access();
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
        if (taskMemEnd >= mem) {
            tasks.push_back({mem, static_cast<size_t>(memEnd - mem)});
            break;
        }

        string_view right{taskMemEnd, static_cast<size_t>(memEnd - taskMemEnd)};//预期位置后面的buf
        auto newLinePos = right.find('\n');
        //根据newLine切出合适的task
        if (newLinePos == string_view::npos) {
            tasks.push_back({mem, static_cast<size_t>(memEnd - mem)});
            break;
        } else {
            tasks.push_back({mem, newLinePos + 1});
            mem += (newLinePos + 1);
        }
    }
    
    //提交任务
    auto veryBegin = (const char*)mMapInfo.addr;
    auto ret = calculation.schedule(move(tasks), bind(&FileLog::buildBlock, placeholders::_1, placeholders::_2, veryBegin));
    ret->then([this](shared_ptr<Promise> p){
                    if (!p->isCancelled()) {
                        this->collectBlocks(p->calculationValue());
                    }
                }, false);
    return ret;
}

any FileLog::buildBlock(bool* cancel, string_view buf, const char* veryBegin) {
    auto begin = buf.begin();
    auto end = buf.end();

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
            curBlock = createBlock(SV_CPP20_ITER(last));
        }

        auto [newline, next] = findNewLine(last, end);

        curBlock->lines.push_back({
            static_cast<BlockCharI>(SV_CPP20_ITER(last) - (veryBegin + curBlock->offset)),
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

#include "stdout.h"

void FileLog::createFileSizeWatcher() {
    auto path = mPath;
    auto size = mMapInfo.len;

    mFileSizeWatcher = PingTask::create(
        [=]{return filesystem::file_size(path) > size;},
        [=] {
            //预期500ms一个文件的大小变化不会非常巨大，所以我们用单线程处理(不直接在主线程处理，避免卡住主线程)
            thread(&FileLog::onFileNewContent, this).detach();
            //先停止watcher，避免反复触发
            return false;
        }
    );

    mFileSizeWatcher->start(500);
}

static void merge(vector<Block*>& target, const vector<Block*>& source, LogLineI& count) {
    auto& lastBlock = LastItem(target);
    auto freeSpace = BLOCK_LINE_NUM - lastBlock->lines.size();

    auto it = source.begin();
    //合并到最后一个block，要么整个吞，要么不吞
    while (it != source.end() && freeSpace > (*it)->lines.size()) {
        auto from = (*it)->lines.begin();
        auto to = (*it)->lines.end();
        auto blockDiff = (*it)->offset - lastBlock->offset;
        for_each(from, to, [lastBlock, blockDiff](Line& l) {
            l.offset += blockDiff;
            lastBlock->lines.push_back(l);
        });
        count += (*it)->lines.size();
        ++it;
    }
    
    //剩下的block自立门户
    for_each(it, source.end(), [&count](Block* b){
        b->lineBegin = count;
        count += b->lines.size();
    });

    copy(it, source.end(), back_inserter(target));
}

//在一个独立线程中执行
void FileLog::onFileNewContent() {
    auto oldSize = mMapInfo.len;
    auto newSize = filesystem::file_size(mPath);
    auto mapInfo = createMapOfFile(mPath);
    if (!mapInfo.valid()) {
        LOGW("file size changed detected, but map failed");
        return;
    }
    string_view viewOfNewContent((const char*)mapInfo.addr + oldSize, newSize - oldSize);
    
    bool cancel =false;
    auto blocks = any_cast<BlockChain>(buildBlock(&cancel, viewOfNewContent, (const char*)mapInfo.addr));

    if (blocks.empty()) {
        createFileSizeWatcher();
        return;
    }

    EventLoop::instance().post(EventType::Write, [this, blocks]{
        //模拟关闭后重新打开（只是不清除之前的blocks信息）
        unmapFile(mMapInfo);
        if (!open(mPath)) {
            //FIXME:这里抛出的错误，并不会立即被检测到，在被检测到之前，如果有访问到内存的行为，将是不可预期的！！
            activateAttr(LOG_ATTR_MAY_DISCONNECT, true);
            return Promise::resolved(false);
        }

        //追加新的行信息
        merge(mBlocks, blocks, mCount);
        activateAttr(LOG_ATTR_DYNAMIC_RANGE, true);

        //重新开始监听文件变化
        createFileSizeWatcher();

        return Promise::resolved(true);
    });
}