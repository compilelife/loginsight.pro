#include "gtest/gtest.h"
#include "filelog.h"
#include <chrono>
#include <filesystem>
#include <stdio.h>

#include "stdout.h"

using namespace std::chrono;
using namespace std::filesystem;

class MeasureTime {
private:
    time_point<steady_clock> mFrom;
    string_view mName;
public:
    MeasureTime(string_view name) {
        mFrom = steady_clock::now();
        mName = name;
    }

    ~MeasureTime() {
        auto now = steady_clock::now();
        auto cost = now - mFrom;

        auto duration = duration_cast<milliseconds>(cost);

        LOGI("%s cost %dms", mName.data(), duration.count());
    }
};

void prepareHugeLog() {
    LOGI("prepareHugeLog...")
    if (exists("./huge.log"))
        return;

    auto size = file_size("./sample.log");
    auto count = 2 * 1024 * 1024 * 1024L / size + 1;

    auto f = fopen("./sample.log", "r");
    auto buf = new char[size];
    fread(buf, 1, size, f);
    fclose(f);

    auto fhuge = fopen("./huge.log", "w+");

    for (size_t i = 0; i < count; i++)
    {
        fwrite(buf, 1, size, fhuge);
    }

    fclose(fhuge);

    LOGI("prepareHugeLog done")
}

//11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz
//m2 固态硬盘; 日志2GB； release编译
//文件已cache: 400ms
//文件未cache： 29395 (rm huge.log)
#if 0
TEST(Benchmark, openFile) {
    prepareHugeLog();
    FileLog log;

    ASSERT_TRUE(log.open("./huge.log"));

    MeasureTime mt("openFile");

    log.scheduleBuildBlocks()
        ->wait();
}
#endif