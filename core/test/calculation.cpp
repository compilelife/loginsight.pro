#include "calculation.h"
#include "gtest/gtest.h"
#include "stdout.h"
#include <chrono>
#include <thread>
#include <filesystem>
#include "eventloop.h"

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
    p->then([&thenExecuted](auto){thenExecuted=true;},false);

    ASSERT_TRUE(p->isBusy());

    p->cancel();

    ASSERT_TRUE(p->isCancelled());
    ASSERT_FALSE(p->isBusy());
    ASSERT_TRUE(thenExecuted);
}

static const string_view foo = "Loginsight a great tool to analyze LOG";

TEST(Calculation, filterByString) {
    auto f = createFilter("loginsight", false);
    ASSERT_TRUE(f(foo));

    f = createFilter("loginsight", true);
    ASSERT_FALSE(f(foo));
}

TEST(Calculation, filterByRegex) {
    regex caseSense(R"(l.g)");
    auto f = createFilter(caseSense);
    ASSERT_FALSE(f(foo));

    regex caseUnsense(R"(l.g)", regex_constants::icase);
    f = createFilter(caseUnsense);
    ASSERT_TRUE(f(foo));
}

TEST(Calculation, findByString) {
    auto r1 = createFind("log", false, false)(foo);
    ASSERT_EQ((FindRet{0,3}), r1);
    auto r2 = createFind("log", true, false)(foo);
    ASSERT_FALSE(r2);
    auto r3 = createFind("log", false, true)(foo);
    ASSERT_EQ((FindRet{35,3}), r3);
}

TEST(Calculation, findByRegex) {
    auto r1 = createFind(regex{"l.g", regex_constants::icase}, false)(foo);
    ASSERT_EQ((FindRet{0,3}), r1);
    auto r2 = createFind(regex{"l.g"}, false)(foo);
    ASSERT_FALSE(r2);
    auto r3 = createFind(regex{"l.g", regex_constants::icase}, true)(foo);
    ASSERT_EQ((FindRet{35,3}), r1);
}