#include "../platform.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include "../stdout.h"
#include <sys/wait.h>

struct PrivMapInfo {
    int fd;  
};

MMapInfo createMapOfFile(string_view path, uint64_t offset, uint64_t size) {
    MMapInfo ret;

    PrivMapInfo priv;
    priv.fd = open(path.data(), O_RDONLY);
    if (priv.fd < 0) {
        return {priv, nullptr};
    }

    ret.priv = priv;
    ret.len = size > 0 ? size : (std::filesystem::file_size(path) - offset);
    ret.addr = mmap(nullptr, ret.len, PROT_READ, MAP_SHARED, priv.fd, offset);

    return ret;
}

void unmapFile(MMapInfo& info) {
    auto priv = std::any_cast<PrivMapInfo>(info.priv);
    munmap(info.addr, info.len);
    close(priv.fd);

    info.addr = nullptr;
}

struct PrivProcessInfo {
    pid_t pid;
    FILE* fp;
};

#define READ   0
#define WRITE  1
FILE * popen2(string command, string type, int & pid)
{
    pid_t child_pid;
    int fd[2];
    pipe(fd);

    if((child_pid = fork()) == -1)
    {
        LOGE("failed to fork");
        return nullptr;
    }

    /* child process */
    if (child_pid == 0)
    {
        if (type == "r")
        {
            close(fd[READ]);    //Close the READ end of the pipe since the child's fd is write-only
            dup2(fd[WRITE], 1); //Redirect stdout to pipe
        }
        else
        {
            close(fd[WRITE]);    //Close the WRITE end of the pipe since the child's fd is read-only
            dup2(fd[READ], 0);   //Redirect stdin to pipe
        }

        setpgid(child_pid, child_pid); //Needed so negative PIDs can kill children of /bin/sh
        execl("/bin/sh", "/bin/sh", "-c", command.c_str(), NULL);
        exit(0);
    }
    else
    {
        if (type == "r")
        {
            close(fd[WRITE]); //Close the WRITE end of the pipe since parent's fd is read-only
        }
        else
        {
            close(fd[READ]); //Close the READ end of the pipe since parent's fd is write-only
        }
    }

    pid = child_pid;

    if (type == "r")
    {
        return fdopen(fd[READ], "r");
    }

    return fdopen(fd[WRITE], "w");
}

int pclose2(FILE * fp, pid_t pid)
{
    int stat;

    fclose(fp);
    kill(pid, 9);
    while (waitpid(pid, &stat, 0) == -1)
    {
        if (errno != EINTR)
        {
            stat = -1;
            break;
        }
    }

    return stat;
}

ProcessInfo openProcess(string_view cmdline) {
    ProcessInfo ret;

    PrivProcessInfo priv;
    priv.fp = popen2(cmdline.data(), "r", priv.pid);

    if (!priv.fp)
        return ret;

    ret.priv = priv;
    ret.stdoutFd = fileno(priv.fp);
    return ret;
}

void closeProcess(ProcessInfo& info) {
    auto priv = any_cast<PrivProcessInfo>(info.priv);
    pclose2(priv.fp, priv.pid);
}

int readFd(int fd, void* buf, int howmuch) {
    return read(fd, buf, howmuch);
}


int getFileNo(FILE* fp) {
    return fileno(fp);
}