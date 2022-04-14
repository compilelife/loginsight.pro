#include "platform.h"
#include <stdio.h>

int getFileNo(FILE* fp) {
    return _fileno(fp);
}