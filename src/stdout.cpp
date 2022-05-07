#include "stdout.h"
#include <stdio.h>
#include <cstdarg>

StdOut::StdOut() {
}

StdOut& StdOut::instance() {
    static StdOut impl;
    return impl;
}

void StdOut::send(constr str) {
    lock_guard<mutex> l(mMutex);
    mLastLine = str;
    printf("%s", str.c_str());
    fflush(stdout);
}

void StdOut::debug(constr fmt, ...) {
    char buf[1024] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt.c_str(), args);
    va_end(args);

    send(buf);
}

constr StdOut::lastLine() {
    return mLastLine;
}