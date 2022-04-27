#include "controller.h"
#include <iostream>
#include "eventloop.h"
#include "platform.h"
#include "stdout.h"
#include "filelog.h"
#include <unordered_map>
#include "sublog.h"

unordered_map<string,CmdHandlerWrap> gCmdHandlers;

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
            try {
                handleCmd(root);
            } catch (Json::LogicError e) {
                LOGE("failed to handle %s: %s", cstr, e.what());
            }
        } else {
            LOGE("invalid cmd: %s", cstr);
            LOGE("reason: %s", errs.c_str());
        }

        free(cstr);

        return true;
    }

    return false;
}
shared_ptr<Promise> Controller::find(shared_ptr<ILog> log, 
                            FindFunction f, 
                            LogLineI fromLine, 
                            LogCharI fromChar, 
                            bool reverse) {
    auto range = log->range();
    auto view = reverse ? log->view(range.begin, fromLine) : log->view(fromLine, range.end);
    if (reverse)
        view->reverse();
    
    //第一行比较特殊，可能在句子的一半位置开始，我们需要特殊处理下
    auto lineRef = view->current();
    auto firstLine = reverse ? (lineRef.str().substr(0, fromChar+1)) : (lineRef.str().substr(fromChar));
    auto firstLineRet = f(firstLine);
    if (firstLineRet) {
        FindRet r{firstLineRet.offset + fromChar, firstLineRet.len};
        return Promise::resolved(FindLogRet{
            lineRef,
            r
        });
    }

    //如果在第一行没有找到匹配，那么我们用多线程查找
    auto unprocessed = view->subview(1, view->size() - 1);
    auto doIterateFind = [f](bool* cancelled, shared_ptr<LogView> view) {
        while (!view->end() && !*cancelled) {
            auto lineRef = view->current();
            auto ret = f(lineRef.str());
            if (ret)
                return FindLogRet{lineRef, ret};
            view->next();
        }
        return FindLogRet{LineRef{}, FindRet::failed()};
    };
    return Calculation::instance().peekFirst(unprocessed, doIterateFind);
}

void Controller::handleCmd(JsonMsg msg) {
    auto cmd = msg["cmd"].asString();
    Json::Value ret;

    if (cmd.empty()) {
        ret = failedAck(msg, "cmd field not found in json");
    } else {
        auto &h = gCmdHandlers[cmd];
        if (h.handler) {
            if (mBarrierPromise && mBarrierPromise->isBusy() && h.waitBarrier) {
                ret = failedAck(msg, "程序在忙,稍后再试");
            } else {
                ret = (this->*(h.handler))(msg);
            }
        }
        else {
            ret = failedAck(msg, "cmd not support");
        }
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

    mBarrierPromise = log->scheduleBuildBlocks();
    mBarrierPromise->then([msg, log, this](shared_ptr<Promise> p){
        if (!handleCancelledPromise(p, msg)) {
            auto ret = ack(msg, ReplyState::Ok);

            auto logId = mLogTree.setRoot(log);
            ret["logId"] = logId;

            log->range().writeTo(ret["range"]);
            send(ret);
        }
    }, true);
    
    return ack(msg, ReplyState::Future);
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

ImplCmdHandler(closeLog) {
    mLogTree.delLog(msg["logId"].as<LogId>());
    return ack(msg, ReplyState::Ok);
}

ImplCmdHandler(filter) {
    auto log = getLog(msg);
    if (!log) {
        return failedAck(msg, "logId not set");
    }

    auto isRegex = msg["regex"].asBool();
    auto pattern = msg["pattern"].asString();
    auto caseSense = msg["caseSense"].asBool();
    FilterFunction filter;
    if (isRegex) {
        filter = caseSense ? createFilter(regex(pattern))
                    : createFilter(regex(pattern, regex_constants::ECMAScript | regex_constants::icase));
    } else {
        filter = createFilter(pattern, caseSense);
    }

    mBarrierPromise = SubLog::createSubLog(log->view(), filter);
    mBarrierPromise->then([log, msg, this](shared_ptr<Promise> p){
        if (!handleCancelledPromise(p, msg)) {
            auto subLog = any_cast<shared_ptr<SubLog>>(p->value());
            auto ret = ack(msg, ReplyState::Ok);
            ret["logId"] = mLogTree.addLog(log, subLog);
            log->range().writeTo(ret["range"]);
            send(ret);
        }
    }, true);

    return ack(msg, ReplyState::Future);
}

ImplCmdHandler(search) {
    auto log = getLog(msg);
    if (!log) {
        return failedAck(msg, "logId not set");
    }

    auto fromLine = msg["fromLine"].asUInt64();
    auto fromChar = msg["fromChar"].asUInt();
    auto reverse = msg["reverse"].asBool();
    auto isRegex = msg["regex"].asBool();
    auto pattern = msg["pattern"].asString();
    auto caseSense = msg["caseSense"].asBool();

    FindFunction f;
    if (isRegex) {
        f = caseSense ? createFind(regex(pattern), reverse)
                    : createFind(regex(pattern, regex_constants::ECMAScript | regex_constants::icase), reverse);
    } else {
        f = createFind(pattern, caseSense, reverse);
    }

    mBarrierPromise = find(log, f, fromLine, fromChar, reverse);
    mBarrierPromise->then([log, msg, this](shared_ptr<Promise> p) {
        if (!handleCancelledPromise(p, msg)) {
            auto findRet = any_cast<FindLogRet>(p->value());
            auto ret = ack(msg, ReplyState::Ok);
            if (findRet.extra) {
                ret["line"] = findRet.line.index();
                ret["offset"] = findRet.extra.offset;
                ret["len"] = findRet.extra.len;
                ret["found"] = true;
            } else {
                ret["found"] = false;
            }
            send(ret);
        }
    }, true);

    return ack(msg, ReplyState::Future);
}

//cmd: cancelPromise