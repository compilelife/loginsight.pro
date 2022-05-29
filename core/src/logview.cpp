#include "logview.h"
#include "log.h"
#include "mem.h"

string_view LineRef::str() {
    auto pmem = refBlock.mem->access();
    auto pstart = pmem + refBlock.block->offset + line->offset;
    return {pstart, line->length};
}

LogLineI LineRef::index() {
    return indexInBlock + refBlock.block->lineBegin;
}