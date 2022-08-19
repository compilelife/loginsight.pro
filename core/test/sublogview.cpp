#include "gtest/gtest.h"
#include <memory>
#include "sublog.h"
#include "log.h"
#include "mem.h"

using namespace std;

//模拟一个根log：两个block, 每个block 10行；以及sublog：过滤其index为奇数的行
static shared_ptr<LogView> prepareMockData() {
    vector<FilterBlock> blocks;
    for (size_t i = 0; i < 2; i++)
    {
        auto block = new Block;
        for (size_t j = 0; j < 10; j++)
        {
            block->lineBegin = 10*i;
            block->lines.push_back({
                (BlockCharI)j, (LineCharI)2
            });
        }
        blocks.push_back({
            {block, nullptr},
            {1,3,5,7,9}
        });
    }
    
    auto log = new SubLog(move(blocks), 10);

    return log->view();
}

TEST(SubLogView, iter){
    auto view = prepareMockData();
    ASSERT_EQ(10, view->size());
    for (size_t i = 0; i < view->size(); i++)
    {
         ASSERT_EQ(i*2+1, view->current().index());
         view->next();
    }
    ASSERT_TRUE(view->end());
}

TEST(SubLogView, subAtBoundary) {
    auto view = prepareMockData();
    auto sub = view->subview(0, 5);
    ASSERT_EQ(5, sub->size());

    sub = view->subview(5, 5);
    ASSERT_EQ(5, sub->size());
}

TEST(SubLogView, subCrossBlock) {
    auto view = prepareMockData();
    auto sub = view->subview(1, 5);
    ASSERT_EQ(5, sub->size());
    for (size_t i = 0; i < 5; i++)
    {
        ASSERT_EQ(i*2+3, sub->current().index());
        sub->next();
    }
    ASSERT_TRUE(sub->end());
}

TEST(SubLogView, reverse) {
    auto view = prepareMockData();
    
    view->reverse();
    auto cur = view->current();
    ASSERT_EQ(10, cur.refBlock.block->lineBegin);
    ASSERT_EQ(9, cur.indexInBlock);

    auto sub = view->subview(5, 2);
    cur = sub->current();

    ASSERT_EQ(0, cur.refBlock.block->lineBegin);
    ASSERT_EQ(9, cur.indexInBlock);
    ASSERT_EQ(2, sub->size());
}