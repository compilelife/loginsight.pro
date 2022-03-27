#include "gtest/gtest.h"
#include "blocklogview.h"
#include "log.h"
#include "mem.h"

pair<vector<Block*>, unique_ptr<Memory>>
prepareMockData() {
    //生成两个Block，每个Block有5行，每行的offset等于行号
    BlockCharI lineNum = 0;
    vector<Block*> blocks;
    for (size_t i = 0; i < 2; i++)
    {
        Block* block = new Block;
        for (size_t j = 0; j < 5; j++)
        {
            block->lines.push_back({lineNum++, 0});
        }
        blocks.push_back(block);
    }
    return {blocks, unique_ptr<Memory>(new Memory)};
}

TEST(BlockLogView, Iter) {
    auto [blocks, mem] = prepareMockData();
    BlockLogView view(blocks, mem.get());

    ASSERT_EQ(10, view.size());
    for (size_t i = 0; i < 10; i++)
    {
        ASSERT_EQ(i, view.current().line->offset);
        view.next();
    }
}

TEST(BlockLogView, subViewNotChanged) {
    auto [blocks, mem] = prepareMockData();
    BlockLogView view(blocks, mem.get());

    auto sub = view.subview(0, 1);
    ASSERT_EQ(5, sub->size());
    for (size_t i = 0; i < 5; i++)
    {
        ASSERT_EQ(i, sub->current().line->offset);
        sub->next();
    }
    
    //view的状态变化不会影响到subview的返回值
    view.next();

    sub = view.subview(0, 1);
    ASSERT_EQ(5, sub->size());
    for (size_t i = 0; i < 5; i++)
    {
        ASSERT_EQ(i, sub->current().line->offset);
        sub->next();
    }
}

TEST(BlockLogView, subViewCrossBlock) {
    auto [blocks, mem] = prepareMockData();
    BlockLogView view(blocks, mem.get());

    auto sub = view.subview(0, 6);
    ASSERT_EQ(10, sub->size());
}

TEST(BlockLogView, subViewNotOvershot) {
    auto [blocks, mem] = prepareMockData();
    //[[x x 2 3 4], [5 6 7 x x]]
    BlockLogView view(blocks, mem.get(), 2, 2);

    auto sub = view.subview(0, 5);
    ASSERT_EQ(6, sub->size());
    for (size_t i = 0; i < 5; i++)
    {
        ASSERT_EQ(i+2, sub->current().line->offset);
        sub->next();
    }
}