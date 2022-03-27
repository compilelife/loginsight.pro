#include "gtest/gtest.h"

#include <thread>
#include <cstdlib>
#include <chrono>

#include "stdout.h"

using namespace std;

TEST(StdOut, MultiThread)
{
    auto randomOutput = [](constr thdname){
        for (size_t i = 0; i < 10; i++)
        {
            this_thread::sleep_for(chrono::milliseconds(rand()%100));
            LOGI("%s: %d", thdname.c_str(), i);
        }
    };

    auto th1 = thread(randomOutput, "thread 1");
    auto th2 = thread(randomOutput, "thread 2");

    th1.join();
    th2.join();
}