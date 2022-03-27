#pragma once

#include "def.h"
#include "logview.h"
#include <functional>
#include <atomic>
#include <future>
#include <any>
#include <memory>

using ThenFunc = function<void(vector<any>&)>;
using FinalFunc = function<void()>;

class Calculation {
private:
    unsigned mCoreNum;
    
    vector<ThenFunc> mThens;
    vector<FinalFunc> mFinals;
    
    future<void> mEndFuture;
    vector<future<any>> mTaskFutures;
    atomic_bool mCanceled;
    bool mEnd{true};
private:
    Calculation();
    vector<unique_ptr<LogView>> split(unique_ptr<LogView>&& iter);
    void prepareSchedule();

public:
    static Calculation& instance();
    unsigned coreNum() {return mCoreNum;}

public:
    bool isBusy();
    bool isCancelled() {return mCanceled;}
    /**
     * @brief cancel会一直阻塞到真正完成; 与wait只能在同一个线程调用
     * 
     */
    void cancel();
    bool wait(long ms = 0);
    /**
     * @brief 在process函数内积极检测isCancelled以在取消时尽快退出
     */
    Calculation& schedule(unique_ptr<LogView> iter, 
            function<any(unique_ptr<LogView>)>&& process);

    Calculation& schedule(vector<string_view> tasks, 
            function<any(string_view)>&& process);

    Calculation& then(ThenFunc&& handler);

    Calculation& finally(FinalFunc&& atLast);

    Calculation& sealed();
};