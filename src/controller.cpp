#include "controller.h"
#include <iostream>
#include "eventloop.h"
#include "platform.h"
#include "stdout.h"
#include "filelog.h"

Controller::Controller() {
    auto& evloop = EventLoop::instance();
    auto evBase = evloop.base();

    mReadStdin = event_new(evBase, getFileNo(stdin), EV_READ|EV_PERSIST, 
        [](evutil_socket_t fd, short, void* arg){
            auto thiz = (Controller*)arg;
            thiz->readStdin(fd);
        }, this);

    mLineBuf = evbuffer_new();

    event_add(mReadStdin, nullptr);
}

Controller::~Controller() {
    event_free(mReadStdin);
    evbuffer_free(mLineBuf);
}

void Controller::start() {}
void Controller::stop() {}

void Controller::readStdin(int fd) {
    LOGI("enter");
    auto readCnt = evbuffer_read(mLineBuf, fd, -1);
    LOGI("read %d", readCnt);

    size_t len = 0;
    auto cstr = evbuffer_readln(mLineBuf, &len, EVBUFFER_EOL_LF);

    if (cstr && len > 0) {
        Json::Reader reader;
        Json::Value root;
        auto parseRet = reader.parse(cstr, root);

        if (parseRet) {
            handleCmd(root);
        } else {
            LOGE("invalid cmd: %s", cstr);
        }

        free(cstr);
    }
}

void Controller::handleCmd(Json::Value& msg) {
    auto cmd = msg["cmd"].asString();
    if (cmd == "openFile") {
        handleCmd(msg);
    }
}

void Controller::handleOpenFile(Json::Value& msg) {
    auto path = msg["path"].asString();
    
    auto log = make_shared<FileLog>();

    if (!log->open(path)) {
        replyFailMsg(msg, "文件打开失败");
        return;
    }

    mLogTree.setRoot(log);

    log->scheduleBuildBlocks();
    /**
     * 1. 将Promise设为barrierPromise
     * 2. 界面接着定时请求barrierPromise的进度
     * 
     */
}