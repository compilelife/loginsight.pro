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
                j, 2
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