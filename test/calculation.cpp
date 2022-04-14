#include "calculation.h"
#include "gtest/gtest.h"
#include "stdout.h"
#include <chrono>
#include <thread>
#include <filesystem>

using namespace std;
using namespace std::chrono_literals;

TEST(Calculation, cancel) {
    vector<string_view> tasks{"1","2"};

    bool thenExecuted = false;

    auto p = Calculation::instance()
        .schedule(tasks, [](bool* cancel, string_view s){
            LOGI("%s", s.data());
            this_thread::sleep_for(300ms);//稍作等待，让cancel有机会在任务完成前被执行到
            return s;
        });
    p->then([&thenExecuted](auto){thenExecuted=true;});

    ASSERT_TRUE(p->isBusy());

    p->cancel();

    ASSERT_TRUE(p->isCancelled());
    ASSERT_FALSE(p->isBusy());
    ASSERT_TRUE(thenExecuted);
}