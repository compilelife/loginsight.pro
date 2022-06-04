#include "controller.h"
#include <iostream>
#include "eventloop.h"
#include "platform.h"
#include "stdout.h"
#include "filelog.h"
#include <unordered_map>
#include "sublog.h"
#include "monitorlog.h"
#include "multifilelog.h"

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
    auto ret = evbuffer_read(mLineBuf, fd, -1);
    if (ret <= 0) {
        LOGE("something wrong with stdin");
        event_del(mReadStdin);
        return;
    }
    while(handleLine()) {}
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

void Controller::handleCmd(JsonMsg msg) {
    auto cmd = msg["cmd"].asString();

    if (cmd.empty()) {
        send(failedAck(msg, "cmd field not found in json"));
    } else {
        if (cmd == "reply") {
            return;//目前还不需要关心客户端的回包，直接丢弃之
        }

        auto &h = gCmdHandlers[cmd];
        if (h.handler) {
            if (h.type == EventType::Immediate) {
                (this->*(h.handler))(msg);
            } else {
                auto taskPromise = EventLoop::instance().post(h.type, [msg, this, h]{return (this->*(h.handler))(msg);});
                if (taskPromise->isBusy()) {
                    auto promiseId = ++mPromiseIdGen;
                    auto reply = ack(msg, ReplyState::Future);
                    reply["pid"] = promiseId;
                    send(reply);

                    mTaskPromises[promiseId] = taskPromise;
                    taskPromise->then([this, promiseId](shared_ptr<Promise> p){
                        mTaskPromises.erase(promiseId);
                    }, true);
                }
            }
        }
        else {
            send(failedAck(msg, "cmd not support"));
        }
    }
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
            if (ret){
                return any(FindLogRet{lineRef, ret});
            }
            view->next();
        }
        return any();//让peekFirst识别该线程未搜索到匹配值
    };
    
    auto p = Calculation::instance().peekFirst(unprocessed, doIterateFind);
    p->then([](shared_ptr<Promise> p){
        if (!p->isCancelled() && !p->value().has_value()) {
            p->setValue(FindLogRet{});//设置失败值
        }
    });

    return p;
}



Json::Value Controller::failedAck(JsonMsg msg, string why) {
    auto reply = ack(msg, ReplyState::Fail);
    reply["why"] = why;
    return reply;
}

Json::Value Controller::ack(JsonMsg msg, ReplyState state) {
    Json::Value root;
    root["cmd"] = "reply";
    root["origin"] = msg["cmd"];
    root["id"] = msg["id"];
    root["state"] = state;
    return root;
}

void Controller::send(JsonMsg msg) {
    Json::FastWriter writer;//writer不是线程安全的，send却会被多个线程调用，所以writer不能是成员变量
    StdOut::instance().send(writer.write(msg));
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

shared_ptr<PingTask> createLogAttrWatcher(shared_ptr<ILog> log, size_t attr, function<bool()>&& handler) {
    return PingTask::create(
        [=]{return log->isAttrAtivated(attr);},
        [=]{
            auto ret = handler();
            log->activateAttr(attr, false);
            return ret;
        }
    );
}

Json::Value Controller::onRootLogReady(JsonMsg msg, shared_ptr<IClosableLog> log) {
    auto range = log->range();
    auto logId = mLogTree.setRoot(log);

    //添加到log tree
    auto ret = ack(msg, ReplyState::Ok);
    ret["logId"] = logId;
    range.writeTo(ret["range"]);

    //构建检测任务
    if (log->hasAttr(LOG_ATTR_DYNAMIC_RANGE)) {
        mWatchRangeTask = createLogAttrWatcher(log, LOG_ATTR_DYNAMIC_RANGE, 
            [this, logId]{
                auto msg = prepareMsg("rangeChanged");
                msg["logId"] = logId;
                send(msg);
                return true;
            });
        mWatchRangeTask->start(50);
    }

    if (log->hasAttr(LOG_ATTR_MAY_DISCONNECT)) {
        mWatchDisconnectTask = createLogAttrWatcher(log, LOG_ATTR_MAY_DISCONNECT, 
            [this, logId]{
                auto msg = prepareMsg("disconnected");
                msg["logId"] = logId;
                send(msg);
                return true;
            });
        mWatchDisconnectTask->start(50);
    }

    return ret;
}

void Controller::onRootLogFinalize() {
    if (mWatchDisconnectTask) {
        mWatchDisconnectTask->stop();
        mWatchDisconnectTask.reset();
    }
    if (mWatchRangeTask) {
        mWatchRangeTask->stop();
        mWatchRangeTask.reset();
    }
}

//{"cmd":"openFile","id":"ui-1","path":"/tmp/1.log"}
ImplCmdHandler(openFile) {
    auto path = msg["path"].asString();
    
    auto log = make_shared<FileLog>();

    if (!log->open(path)) {
        send(failedAck(msg, "文件打开失败"));
        return Promise::resolved(false);
    }

    auto p = log->scheduleBuildBlocks();
    p->then([msg, log, this](shared_ptr<Promise> p){
        if (!handleCancelledPromise(p, msg)) {
            send(onRootLogReady(msg, log));
        }
    }, true);

    return p;
}

ImplCmdHandler(openProcess) {
    auto cmd = msg["process"].asString();

    auto log = make_shared<MonitorLog>();
    auto ret = log->open(cmd, EventLoop::instance().base());

    if (!ret) {
        send(failedAck(msg, "命令运行失败"));
        return Promise::resolved(false);
    }
    
    send(onRootLogReady(msg, log));
    return Promise::resolved(true);
}

ImplCmdHandler(openMultiFile) {
    auto files = msg["files"];
    vector<string_view> paths;
    for (auto i = 0; i < files.size(); i++) {
        paths.push_back(files[i].asString());
    }
    
    auto log = make_shared<MultiFileLog>();
    if (!log->open(paths)) {
        send(failedAck(msg, "文件打开失败"));
        return Promise::resolved(false);
    }

    auto p = log->scheduleBuildBlocks();
    p->then([msg, log, this](shared_ptr<Promise> p){
        if (!handleCancelledPromise(p, msg)) {
            send(onRootLogReady(msg, log));
        }
    }, true);
    
    return p;
}

//{"cmd":"queryPromise", "id":"ui-2"}
ImplCmdHandler(queryPromise) {
    auto pid = msg["pid"];
    if (!pid) {
        send(failedAck(msg, "promise id not set"));
        return Promise::resolved(false);
    }

    auto reply = ack(msg, ReplyState::Ok);

    auto it = mTaskPromises.find(pid.asInt());
    if (it == mTaskPromises.end()) {
        reply["progress"] = 100;
    } else  {
        auto& p = (*it).second;
        reply["progress"] = p->isBusy() ? 0 : 100;
    }
    
    send(reply);

    return Promise::resolved(true);
}


ImplCmdHandler(cancelPromise) {
    auto pid = msg["pid"];
    if (!pid) {
        send(failedAck(msg, "promise id not set"));
        return Promise::resolved(false);
    }

    auto it = mTaskPromises.find(pid.asInt());
    if (it == mTaskPromises.end()) {
        send(failedAck(msg, "promise not exist"));
        return Promise::resolved(false);
    }

    auto& p = (*it).second;

    p->cancel();
    send(ack(msg, ReplyState::Ok));

    return Promise::resolved(true);
}

//{"cmd":"getRange", "id": "ui-3", "logId": 1}
ImplCmdHandler(getRange) {
    auto log = getLog(msg);
    if (!log) {
        send(failedAck(msg, "logId not set"));
        return Promise::resolved(false);
    }

    auto ret = ack(msg, ReplyState::Ok);
    log->range().writeTo(ret["range"]);
    send(ret);

    return Promise::resolved(true);
}

//{"cmd":"getLines", "id": "ui-4", "logId": 1, "range": {"begin": 0, "end": 1}}
ImplCmdHandler(getLines) {
    auto log = getLog(msg);
    if (!log) {
        send(failedAck(msg, "logId not set"));
        return Promise::resolved(false);
    }
    
    auto from = msg["range"]["begin"].asUInt64();
    auto to = msg["range"]["end"].asUInt64();

    return Promise::from([=](bool* cancel){
        auto view = log->view(from, to);
        vector<Json::Value> lineArray;
        auto index = from;

        while(!view->end() && !*cancel) {
            auto cur = view->current();
            string str(cur.str());
            if (!cur.line->segs.has_value()) {
                cur.line->segs = mLineSegment.formatLine(str);
            }
            Json::Value retLine;
            retLine["line"] = cur.index();
            retLine["index"] = index++;
            retLine["content"] = str;//TODO: iconv to utf-8
            retLine["segs"].resize(0);//如果没有segs，则返回空数组
            for (auto&& seg : cur.line->segs.value_or(vector<LineSeg>())) {
                Json::Value v;
                v["offset"] = seg.offset;
                v["length"] = seg.length;
                retLine["segs"].append(v);
            }
            lineArray.push_back(retLine);
            view->next();
        }
            
        if (*cancel) {
            send(ack(msg, ReplyState::Cancel));
        } else {
            auto ret = ack(msg, ReplyState::Ok);
            for (auto &&line : lineArray) {
                ret["lines"].append(line);
            }
            
            send(ret);
        }

        return 0;
    });
}

ImplCmdHandler(closeLog) {
    onRootLogFinalize();
    mLogTree.delLog(msg["logId"].as<LogId>());
    send(ack(msg, ReplyState::Ok));
    return Promise::resolved(true);
}

ImplCmdHandler(filter) {
    auto log = getLog(msg);
    if (!log) {
        send(failedAck(msg, "logId not set"));
        return Promise::resolved(false);
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

    auto p = SubLog::createSubLog(log, filter);
    p->then([log, msg, this](shared_ptr<Promise> p){
        if (!handleCancelledPromise(p, msg)) {
            auto subLog = any_cast<shared_ptr<SubLog>>(p->value());
            auto ret = ack(msg, ReplyState::Ok);
            ret["logId"] = mLogTree.addLog(log, subLog);
            subLog->range().writeTo(ret["range"]);
            send(ret);
        }
    }, true);
    
    return p;
}

ImplCmdHandler(search) {
    auto log = getLog(msg);
    if (!log) {
        send(failedAck(msg, "logId not set"));
        return Promise::resolved(false);
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

    auto p = find(log, f, fromLine, fromChar, reverse);
    p->then([log, msg, this](shared_ptr<Promise> p) {
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

    return p;
}

ImplCmdHandler(listFiles) {
    auto pattern = msg["pattern"].asString();
    auto caseSense = msg["caseSense"].asBool();
    auto compareNum = msg["compareNum"].asBool();
    auto path = msg["path"].asString();

    regex r{pattern, caseSense ? regex_constants::ECMAScript : regex_constants::icase};

    auto files = compareNum ? listFiles<true>(path, r) : listFiles<false>(path, r);

    auto ret = ack(msg, ReplyState::Ok);
    for (auto &&file : files)
        ret["paths"].append(file);
    
    send(ret);
    return Promise::resolved(true);
}

//{lines:[{logid:1,line:2}]}
ImplCmdHandler(mapLine) {
    auto log = getLog(msg);
    if (!log) {
        send(failedAck(msg, "logId not set"));
        return Promise::resolved(false);
    }

    auto line = msg["line"].as<LogLineI>();

    auto srcLine = log->mapToSource(line);
    auto allLines = mLogTree.mapLine(srcLine);

    auto ret = ack(msg, ReplyState::Ok);
    for (auto &&e : allLines) {
        Json::Value item;
        item["logId"] = e.first;
        item["line"] = e.second;
        ret["lines"].append(item);
    }
    
    send(ret);
    return Promise::resolved(true);
}

ImplCmdHandler(setLineSegment) {
    auto pattern = msg["pattern"].asString();
    auto caseSense = msg["caseSense"].asBool();

    auto flag = regex_constants::ECMAScript;
    if (caseSense)
        flag |= regex_constants::icase;
    regex r(pattern, flag);

    vector<Segment> segs;
    for (Json::ArrayIndex i = 0; i < msg["segs"].size(); i++) {
        auto& seg = msg["segs"][i];
        Segment item = {
            SegType(seg["type"].asUInt()),
            seg["name"].asString()
        };
        if (item.type == SegType::Date) {
            item.extra = seg["extra"].asString();
        } else if (item.type == SegType::LogLevel) {
            auto it = seg["extra"].begin();
            auto end = seg["extra"].end();
            LogLevelExtra extra;
            while (it != end) {
                auto key = it.key().asString();
                auto value = it->asInt();
                extra[key] = value;
            }
            item.extra = extra;
        }
        segs.push_back(item);
    }

    if (mLineSegment.hasPatternSet()) {
        auto view = mLogTree.root()->view();
        while (!view->end()) {
            view->current().line->segs.reset();
            view->next();
        }
    }

    mLineSegment.setPattern(r);
    mLineSegment.setSegments(move(segs));

    send(ack(msg, ReplyState::Ok));
    return Promise::resolved(true);
}

string Controller::nextId() {
    return "core-"+to_string(++mIdGen);
}

Json::Value Controller::prepareMsg(string_view cmd) {
    Json::Value v;
    v["cmd"] = string(cmd);
    v["id"] = nextId();
    return v;
}

ImplCmdHandler(syncLogs) {
    auto p = Promise::from([this, msg](bool* cancel){
        map<LogId, Range> ranges;
        mLogTree.travel([cancel, &ranges](Node* n){
            if (*cancel) {
                return true;
            }

            if (ranges.empty()) {//this is root
                ranges[n->id] = n->log->range();
                return false;
            }

            auto sub = (SubLog*)(n->log.get());
            sub->syncParent();
            ranges[n->id] = sub->range();

            return false;
        });

        return ranges;
    });

    p->then([msg, this](shared_ptr<Promise> p){
        if (!handleCancelledPromise(p, msg)) {
            auto ranges = any_cast<map<LogId, Range>>(p->value());

            auto reply = ack(msg, ReplyState::Ok);
            reply["ranges"].resize(0);//ranges数组初始化为[]
            for (auto &&e : ranges) {
                Json::Value v;
                v["logId"] = e.first;
                e.second.writeTo(v["range"]);
                reply["ranges"].append(v);
            }
            send(reply);
        }
    });

    return p;
}