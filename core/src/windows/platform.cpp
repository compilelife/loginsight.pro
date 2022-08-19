#include "../platform.h"
#include <stdio.h>
#include <winsock.h>
#include <Windows.h>

MMapInfo createMapOfFile(string_view path) {
    //https://qa.1r1g.com/sf/ask/692269021/
}
void unmapFile(MMapInfo& info);

ProcessInfo openProcess(string_view cmdline);
void closeProcess(ProcessInfo& info);

int readFd(int fd, void* buf, int howmuch) {
    return recv(fd, buf, howmuch, 0);
}

int getFileNo(FILE* fp) {
    return _fileno(fp);
}