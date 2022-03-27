#pragma once

#include "log.h"
#include <functional>

using SubLogFilter = function<bool(LineRef)>;

class SubLog: public ILog {
public:
public:
    LogView view() const override;
};