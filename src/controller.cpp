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

    //TODO: check barrier and queue cmds

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

void Controller::handleCmd(JsonMsg msg) {
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

Json::Value Controller::failedAck(JsonMsg msg, string why) {
    auto reply = ack(msg, ReplyState::Fail);
    reply["why"] = why;
    return reply;
}

Json::Value Controller::ack(JsonMsg msg, ReplyState state) {
    Json::Value root;
    root["cmd"] = "reply";
    root["id"] = msg["id"];
    root["state"] = state;
    return root;
}

void Controller::send(JsonMsg msg) {
    StdOut::instance().send(mRPCWriter.write(msg));
}

shared_ptr<ILog> Controller::getLog(JsonMsg msg) {
    auto logId = msg["logId"].asInt();
    return mLogTree.getLog(logId);
}

bool Controller::handleCancelledPromise(shared_ptr<Promise>& p, JsonMsg msg) {
    if (p->isCancelled()) {
        send(ack(msg, ReplyState::Cancel));
        return true;
    }

    return false;
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
    mBarrierPromise->then([msg, log, this](shared_ptr<Promise> p){
        if (!handleCancelledPromise(p, msg)) {
            auto ret = ack(msg, ReplyState::Ok);
            log->range().writeTo(ret["range"]);
            send(ret);
        }
    }, true);
    
    auto ret = ack(msg, ReplyState::Future);
    ret["logId"] = logId;

    return ret;
}

//{"cmd":"queryPromise", "id":"ui-2"}
ImplCmdHandler(queryPromise) {
    //TODO: 支持根据Promise id查询
    auto reply = ack(msg, ReplyState::Ok);

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
    auto log = getLog(msg);
    if (!log) {
        return failedAck(msg, "logId not set");
    }

    auto ret = ack(msg, ReplyState::Ok);
    log->range().writeTo(ret["range"]);

    return ret;
}

//{"cmd":"getLines", "id": "ui-4", "logId": 1, "range": {"begin": 0, "end": 1}}
ImplCmdHandler(getLines) {
    auto log = getLog(msg);
    if (!log) {
        return failedAck(msg, "logId not set");
    }
    
    auto from = msg["range"]["begin"].asUInt64();
    auto to = msg["range"]["end"].asUInt64();

    auto lineViews = log->view(from, to);
    mBarrierPromise = Calculation::instance().schedule(lineViews, 
        [](bool* cancel, shared_ptr<LogView> view){
        vector<Json::Value> lineArray;
        while(!view->end() && !*cancel) {
            auto cur = view->current();
            if (!(cur.line->segs)) {
                //TODO: 格式化这行
            }
            Json::Value retLine;
            retLine["content"] = string(cur.str());//TODO: iconv to utf-8
            //FIXME: segs解析仍为空怎么办？
            for (auto&& seg : cur.line->segs.value_or(vector<Seg>())) {
                //TODO: 补充fields
            }
            lineArray.push_back(retLine);
            view->next();
        }
        return lineArray;
    });

    mBarrierPromise->then([this, msg](shared_ptr<Promise> p){
        if (!handleCancelledPromise(p, msg)) {
            auto jsonLines = Calculation::flat<Json::Value>(p->calculationValue());

            auto ret = ack(msg, ReplyState::Ok);
            for (auto &&line : jsonLines) {
                ret["lines"].append(line);
            }
            
            send(ret);
        }
    }, true);

    return ack(msg, ReplyState::Future);
}

//cmd: cancelPromise