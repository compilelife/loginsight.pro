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
    bool finalExecuted = false;

    Calculation::instance()
        .schedule(tasks, [](string_view s){
            LOGI("%s", s.data());
            this_thread::sleep_for(300ms);//稍作等待，让cancel有机会在任务完成前被执行到
            return s;
        })
        .then([&thenExecuted](vector<any>){thenExecuted=true;})
        .finally([&finalExecuted]{finalExecuted=true;})
        .sealed();

    ASSERT_FALSE(Calculation::instance().isCancelled());
    ASSERT_TRUE(Calculation::instance().isBusy());

    Calculation::instance().cancel();

    ASSERT_TRUE(Calculation::instance().isCancelled());
    ASSERT_FALSE(Calculation::instance().isBusy());
    ASSERT_FALSE(thenExecuted);
    ASSERT_TRUE(finalExecuted);
}