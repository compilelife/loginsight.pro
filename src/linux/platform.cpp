#include "../platform.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>
#include <stdio.h>

struct PrivMapInfo {
    int fd;  
};

MMapInfo createMapOfFile(string_view path) {
    MMapInfo ret;

    PrivMapInfo priv;
    priv.fd = open(path.data(), O_RDONLY);
    if (priv.fd < 0) {
        return {priv, nullptr};
    }

    ret.priv = priv;
    ret.len = std::filesystem::file_size(path);
    ret.addr = mmap(nullptr, ret.len, PROT_READ, MAP_SHARED, priv.fd, 0);

    return ret;
}

void unmapFile(MMapInfo& info) {
    auto priv = std::any_cast<PrivMapInfo>(info.priv);
    munmap(info.addr, info.len);
    close(priv.fd);

    info.addr = nullptr;
}

struct PrivProcessInfo {
    FILE* fp;
};

ProcessInfo openProcess(string_view cmdline) {
    ProcessInfo ret;

    PrivProcessInfo priv;
    priv.fp = popen(cmdline.data(), "r");

    if (priv.fp == nullptr) {
        return {priv, -1};
    }

    ret.priv = priv;
    ret.stdoutFd = fileno(priv.fp);
    
    fcntl(ret.stdoutFd, F_SETFL, O_NONBLOCK);

    return ret;
}

void closeProcess(ProcessInfo& info) {
    auto priv = any_cast<PrivProcessInfo>(info.priv);
    pclose(priv.fp);
}