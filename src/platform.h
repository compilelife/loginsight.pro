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


struct ProcessInfo
{
    any priv;
    int stdoutFd;
};

ProcessInfo openProcess(string_view cmdline);
void closeProcess(ProcessInfo& info);
    //http://www.xilixili.net/2019/08/23/run-command-with-output-and-exit-code/
