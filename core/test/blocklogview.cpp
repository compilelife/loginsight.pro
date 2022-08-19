#include "gtest/gtest.h"
#include <memory>
#include "blocklogview.h"
#include "log.h"
#include "mem.h"

using namespace std;

//两个block，每个block有5行，每个line.offset = indexInBlock
static BlockLogView prepareMockData() {
    vector<Block*> blocks;
    for (size_t i = 0; i < 2; i++)
    {
        auto block = new Block;
        for (size_t j = 0; j < 5; j++)
        {
            block->lineBegin = 5*i;
            block->lines.push_back({
                (BlockCharI)j, (LineCharI)2
            });
        }
        blocks.push_back(block);
    }
    return {blocks, nullptr};
}

TEST(BlockLogView, iter){
    auto view = prepareMockData();
    ASSERT_EQ(10, view.size());
    for (size_t i = 0; i < view.size(); i++)
    {
         ASSERT_EQ(i, view.current().index());
         view.next();
    }
    ASSERT_TRUE(view.end());
}

TEST(BlockLogView, subAtBoundary) {
    auto view = prepareMockData();
    auto sub = view.subview(0, 5);
    ASSERT_EQ(5, sub->size());

    sub = view.subview(5, 5);
    ASSERT_EQ(5, sub->size());
}

TEST(BlockLogView, subCrossBlock) {
    auto view = prepareMockData();
    auto sub = view.subview(1, 5);
    ASSERT_EQ(5, sub->size());
    for (size_t i = 0; i < 5; i++)
    {
        ASSERT_EQ(i+1, sub->current().index());
        sub->next();
    }
    ASSERT_TRUE(sub->end());
}

TEST(BlockLogView, reverse) {
    auto view = prepareMockData();
    
    view.reverse();
    auto cur = view.current();
    ASSERT_EQ(5, cur.refBlock.block->lineBegin);
    ASSERT_EQ(4, cur.indexInBlock);

    auto sub = view.subview(5, 2);
    cur = sub->current();

    ASSERT_EQ(0, cur.refBlock.block->lineBegin);
    ASSERT_EQ(4, cur.indexInBlock);
    ASSERT_EQ(2, sub->size());
}