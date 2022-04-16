#include "controller.h"
#include <iostream>
#include "eventloop.h"
#include "platform.h"
#include "stdout.h"
#include "filelog.h"
#include <unordered_map>

unordered_map<string,CmdHandler> gCmdHandlers;

Controller::Controller() {
    auto& evloop = EventLoop::instance();
    auto evBase = evloop.base();

    mReadStdin = event_new(evBase, getFileNo(stdin), EV_READ|EV_PERSIST, 
        [](evutil_socket_t fd, short, void* arg){
            auto thiz = (Controller*)arg;
            thiz->readStdin(fd);
        }, this);

    mLineBuf = evbuffer_new();
}

Controller::~Controller() {
    event_free(mReadStdin);
    evbuffer_free(mLineBuf);
}

void Controller::start() {
    event_add(mReadStdin, nullptr);
}

void Controller::stop() {
    event_del(mReadStdin);
}

void Controller::readStdin(int fd) {
    evbuffer_read(mLineBuf, fd, -1);
    handleLine();
}

void Controller::mockInput(string_view s) {
    evbuffer_add_printf(mLineBuf, "%s\n", s.data());
    while (handleLine()) {}
}

bool Controller::handleLine() {
    size_t len = 0;
    auto cstr = evbuffer_readln(mLineBuf, &len, EVBUFFER_EOL_LF);

    if (cstr && len > 0) {
        Json::Value root;
        Json::String errs;
        stringstream ss(cstr);
        auto parseRet = Json::parseFromStream(mRPCReaderBuilder, ss, &root, &errs);

        if (parseRet) {
            handleCmd(root);
        } else {
            LOGE("invalid cmd: %s", cstr);
            LOGE("reason: %s", errs.c_str());
        }

        free(cstr);

        return true;
    }

    return false;
}

void Controller::handleCmd(Json::Value& msg) {
    auto cmd = msg["cmd"].asString();//堆栈看循环调了这个
    Json::Value ret;

    if (cmd.empty()) {
        ret = failedAck(msg, "cmd field not found in json");
    } else {
        auto &handler = gCmdHandlers[cmd];
        if (handler)
            ret = (this->*handler)(msg);
        else
            ret = failedAck(msg, "cmd not support");
    }

    send(ret);
}

Json::Value Controller::promiseAck(Json::Value& msg) {
    auto reply = ack(msg, true);
    reply["promise"] = true;
    return reply;
}

Json::Value Controller::failedAck(Json::Value& msg, string why) {
    auto reply = ack(msg, false);
    reply["why"] = why;
    return reply;
}

Json::Value Controller::ack(Json::Value& msg, bool success) {
    Json::Value root;
    root["cmd"] = "reply";
    root["id"] = msg["id"];
    root["success"] = success;
    return root;
}

void Controller::send(Json::Value& msg) {
    StdOut::instance().send(mRPCWriter.write(msg));
}

//{"cmd":"openFile","id":"ui-1","path":"/tmp/1.log"}
ImplCmdHandler(openFile) {
    auto path = msg["path"].asString();
    
    auto log = make_shared<FileLog>();

    if (!log->open(path)) {
        return failedAck(msg, "文件打开失败");
    }

    auto logId = mLogTree.setRoot(log);

    mBarrierPromise = log->scheduleBuildBlocks();
    
    auto ret = promiseAck(msg);
    ret["logId"] = logId;

    return ret;
}

//{"cmd":"queryPromise", "id":"ui-2"}
ImplCmdHandler(queryPromise) {
    //TODO: 支持根据Promise id查询
    auto reply = ack(msg, true);

    int progress = 100;
    if (mBarrierPromise) {
        //TODO: 支持查询promise进度
        progress = mBarrierPromise->isBusy() ? 0 : 100;
    }

    reply["progress"] = progress;
    return reply;
}

//{"cmd":"getRange", "id": "ui-3", "logId": 1}
ImplCmdHandler(getRange) {
    auto logId = msg["logId"].asInt();
    auto log = mLogTree.getLog(logId);
    auto range = log->range();

    auto ret = ack(msg, true);
    ret["range"]["begin"]=range.begin;
    ret["range"]["end"]=range.end;

    return ret;
}