#pragma once

#include <any>
#include <string_view>
#include "def.h"

struct MMapInfo
{
    any priv;
    void* addr {nullptr};
    LogCharI len;

    bool valid() { return addr != nullptr; }
};


MMapInfo createMapOfFile(string_view path);
void unmapFile(MMapInfo& info);