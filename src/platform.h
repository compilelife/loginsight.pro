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
    int stdoutFd{-1};
};

ProcessInfo openProcess(string_view cmdline);
void closeProcess(ProcessInfo& info);

int readFd(int fd, void* buf, int howmuch);