#include "logview.h"
#include "log.h"
#include "mem.h"

string_view LineRef::str() {
    auto pmem = refBlock.mem->unsafeAccess();
    auto pstart = pmem + refBlock.block->offset + line->offset;
    return {pstart, line->length};
}