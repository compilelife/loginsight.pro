#include "../platform.h"
#include <stdio.h>
#include <winsock.h>
#include <Windows.h>
#include <filesystem>

struct PrivMapInfo {
    HANDLE fd;
    HANDLE mapHandle;
};

//https://www.cnblogs.com/spencer24/archive/2012/08/03/2621786.html
//https://blog.csdn.net/qiuchangyong/article/details/82219615
MMapInfo createMapOfFile(string_view path) {
    HANDLE fd = CreateFileA(path.data(),
                     GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL,
                     OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL,
                     NULL);
    HANDLE mapHandle = CreateFileMappingW(fd,
                     NULL,
                     PAGE_READONLY,
                     0,
                     0,
                     NULL);
    void* mappedFileAddress = MapViewOfFile(mapHandle,
                    FILE_MAP_ALL_ACCESS,
                    0,
                    0,
                    MMAP_ALLOCATOR_SIZE);

    PrivMapInfo priv;
    priv.fd = fd;
    priv.mapHandle = mapHandle;

    MMapInfo info;
    info.priv = priv;
    info.addr = mappedFileAddress;
    info.len = std::filesystem::file_size(path);
    
    return info;
}

void unmapFile(MMapInfo& info) {
    auto priv = std::any_cast<PrivMapInfo>(info.priv);
    UnmapViewOfFile(info.addr);
    CloseHandle(priv.mapHandle);
    CloseHandle(priv.fd);
}

//https://blog.csdn.net/zyhse/article/details/110695545
struct PrivProcessInfo {
    FILE* fp;
};

ProcessInfo openProcess(string_view cmdline) {
    auto fp = _popen(cmdline.data(), "r");

    PrivProcessInfo priv = {fp};
    ProcessInfo info;
    info.priv = priv;
    info.stdoutFd = _fileno(fp);

    return fp;
}

void closeProcess(ProcessInfo& info) {
    auto priv = std::any_cast<PrivProcessInfo>(info.priv);
    _pclose(priv.fp);
}

int readFd(int fd, void* buf, int howmuch) {
    return recv(fd, buf, howmuch, 0);
}

int getFileNo(FILE* fp) {
    return _fileno(fp);
}