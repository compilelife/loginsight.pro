#pragma once

#include <string>
#include <mutex>

using namespace std;

using constr = const string&;

//统一管理标准输出，避免多线程问题  
class StdOut {
private:
    mutex mMutex;
    string mLastLine;
private:
    StdOut();
public:
    static StdOut& instance();
public:
    void send(constr str);
    void debug(constr fmt, ...);
    constr lastLine();
};

#define LOGI(fmt, ...) StdOut::instance().debug("[%s:%d][INFO] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);
#define LOGW(fmt, ...) StdOut::instance().debug("[%s:%d][WARN] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);
#define LOGE(fmt, ...) StdOut::instance().debug("[%s:%d][ERR] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);