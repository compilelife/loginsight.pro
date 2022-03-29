#pragma once

#include "def.h"
#include "logview.h"
#include <functional>
#include <any>
#include "promisex.h"

using CalculationRet = vector<any>;

class Calculation {
private:
    unsigned mCoreNum;
private:
    Calculation();
    vector<shared_ptr<LogView>> split(shared_ptr<LogView>& iter);
public:
    static Calculation& instance();
    unsigned coreNum() {return mCoreNum;}

public:
    unique_ptr<Promise> schedule(shared_ptr<LogView> iter, 
            function<any(bool*,shared_ptr<LogView>)>&& process);

    unique_ptr<Promise> schedule(vector<string_view> tasks, 
            function<any(bool*,string_view)>&& process);
};