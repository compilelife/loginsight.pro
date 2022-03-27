#include "../platform.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>

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