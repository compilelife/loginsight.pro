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
#include <fstream>
#include <filesystem>
#include "oatpp/encoding/Base64.hpp"
#include <mutex>

using namespace std::filesystem;
using namespace oatpp::encoding;

unordered_map<string,CmdHandlerWrap> gCmdHandlers;
static struct {
    once_flag onceFlag;
    Controller* controller{nullptr};
} StdinData = {};

string Controller::base64Encode(string txt) {
    return enableBase64() ? string(Base64::encode(txt)) : txt;
}

string Controller::base64Decode(string txt) {
    return enableBase64() ? string(Base64::decode(txt)) : txt;
}

string Controller::decodeJsonStr(const Json::Value& v) {
    return base64Decode(v.asString());
}

string Controller::decodePath(string p) {
#ifdef _WIN32
    return p[0] == '/' ? p.substr(1) : p;
#else
    return p;
#endif
}

Controller::Controller() {
}

Controller::~Controller() {
}

void Controller::start() {
    call_once(StdinData.onceFlag, []{
        thread([]{
            while (true) {
                string line;
                getline(cin, line);
                EventLoop::instance().runOnMain([line]{
                    if (StdinData.controller) {
                        StdinData.controller->handleLine(line);
                    }
                });
            }
        }).detach();
    });
    StdinData.controller = this;
}

void Controller::stop() {
    StdinData.controller = nullptr;
}

bool Controller::handleLine(string s) {
    Json::Value root;
    Json::String errs;
    stringstream ss(s);
    auto parseRet = Json::parseFromStream(mRPCReaderBuilder, ss, &root, &errs);

    if (parseRet) {
        try {
            handleCmd(root);
        } catch (Json::LogicError e) {
            LOGE("failed to handle %s: %s", s.c_str(), e.what());
        }
    } else {
        LOGE("invalid cmd: %s", s.c_str());
        LOGE("reason: %s", errs.c_str());
    }

    return true;
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
                    reply["pid"] = (Json::UInt)promiseId;
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
        FindRet r{(LineCharI)(firstLineRet.offset + (reverse ? 0 : fromChar)), firstLineRet.len};
        return Promise::resolved(FindLogRet{
            lineRef,
            r
        });
    }

    if (view->size() <= 1)
    return Promise::resolved(FindLogRet{});

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

bool Controller::canUsePro() {
    return mRegister.getState() != RegisterState::eTryEnd;
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
    if (!canUsePro()) {
        send(failedAck(msg, "试用期已结束。文件打开失败"));
        return Promise::resolved(false);
    }

    auto path = decodePath(decodePath(decodeJsonStr(msg["path"])));

#ifdef OPEN_SOURCE
    if (file_size(path) >= 100*1024*1024) {
        send(failedAck(msg, "开源版只能打开100M以下的文件"));
        return Promise::resolved(false);
    }
#endif
    
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
#ifndef OPEN_SOURCE
    if (!canUsePro()) {
        send(failedAck(msg, "试用期已结束。外部程序打开失败"));
        return Promise::resolved(false);
    }

    auto cmd = decodeJsonStr(msg["process"].asString());
    auto cache = msg["cache"].asInt();

    auto log = make_shared<MonitorLog>();
    auto ret = log->open(cmd, EventLoop::instance().base());

    if (!ret) {
        send(failedAck(msg, "命令运行失败"));
        return Promise::resolved(false);
    }

    if (cache > 0)
        log->setMaxBlockCount(cache);
    
    send(onRootLogReady(msg, log));
#endif
    return Promise::resolved(true);
}

ImplCmdHandler(openMultiFile) {
#ifndef OPEN_SOURCE
    if (!canUsePro()) {
        send(failedAck(msg, "试用期已结束。日志目录打卡失败"));
        return Promise::resolved(false);
    }

    auto files = msg["files"];
    vector<string> paths;
    for (auto i = 0; i < files.size(); i++) {
        paths.push_back(decodePath(decodeJsonStr(files[i])));
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
#endif
    return Promise::resolved(true);
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
            retLine["content"] = base64Encode(str);
            
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
    mLogTree.delLog(msg["logId"].as<Json::UInt>());
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
    auto pattern = decodeJsonStr(msg["pattern"]);
    auto caseSense = msg["caseSense"].asBool();
    auto reverse = msg["reverse"].asBool();

    FilterFunction filter;
    try {
        if (isRegex) {
            filter = caseSense ? createFilter(regex(pattern), reverse)
                        : createFilter(regex(pattern, regex_constants::ECMAScript | regex_constants::icase), reverse);
        } else {
            filter = createFilter(pattern, caseSense, reverse);
        }
    } catch (exception e) {
        send(failedAck(msg, "非法的正则表达式"));
        return Promise::resolved(false);
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
    LOGI("pattern=%s", msg["pattern"].asCString());
    auto pattern = decodeJsonStr(msg["pattern"]);
    auto caseSense = msg["caseSense"].asBool();

    FindFunction f;
    try{
        if (isRegex) {
            f = caseSense ? createFind(regex(pattern), reverse)
                        : createFind(regex(pattern, regex_constants::ECMAScript | regex_constants::icase), reverse);
        } else {
            f = createFind(pattern, caseSense, reverse);
        }
    }catch (exception e) {
        send(failedAck(msg, "非法的正则表达式"));
        return Promise::resolved(false);
    }
    
    auto p = find(log, f, fromLine, fromChar, reverse);
    p->then([log, msg, this](shared_ptr<Promise> p) {
        if (!handleCancelledPromise(p, msg)) {
            auto findRet = any_cast<FindLogRet>(p->value());
            auto ret = ack(msg, ReplyState::Ok);
            if (findRet.extra) {
                ret["index"] = log->fromSource(findRet.line.index());
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

//{lines:[{logid:1,line:2}]}
ImplCmdHandler(mapLine) {
    auto log = getLog(msg);
    if (!log) {
        send(failedAck(msg, "logId not set"));
        return Promise::resolved(false);
    }

    auto index = msg["index"].as<LogLineI>();

    auto srcIndex = log->mapToSource(index);
    auto allIndexes = mLogTree.mapLine(srcIndex);

    auto ret = ack(msg, ReplyState::Ok);
    for (auto &&e : allIndexes) {
        Json::Value item;
        item["logId"] = e.first;
        item["index"] = e.second;
        ret["lines"].append(item);
    }
    
    send(ret);
    return Promise::resolved(true);
}

ImplCmdHandler(setLineSegment) {
#ifndef OPEN_SOURCE
    auto pattern = decodeJsonStr(msg["pattern"]);
    auto caseSense = msg["caseSense"].asBool();

    auto flag = regex_constants::ECMAScript;
    if (!caseSense)
        flag |= regex_constants::icase;
    
    regex r;
    try{
        r = regex(pattern, flag);
    } catch (exception e) {
        send(failedAck(msg, "非法的正则表达式"));
        return Promise::resolved(false);
    }

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

    auto view = mLogTree.root()->view();
    while (!view->end())
    {
        view->current().line->segs.reset();
        view->next();
    }

    mLineSegment.setPattern(r);
    mLineSegment.setSegments(move(segs));

    send(ack(msg, ReplyState::Ok));
#endif

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

ImplCmdHandler(testSyntax) {
    auto pattern = decodeJsonStr(msg["pattern"]);
    auto lines = msg["lines"];

    LineSegment executor;
    try{
        executor.setPattern(regex(pattern));
    }catch(exception e) {
        LOGE("invalid pattern: %s. what: %s", pattern.c_str(), e.what());
        send(failedAck(msg, "非法的正则表达式"));
        return Promise::resolved(false);
    }

    Json::Value result;
    result.resize(0);
    for (Json::Value::ArrayIndex i = 0; i < lines.size(); i++) {
        auto line = decodeJsonStr(lines[i]);
        auto segs = executor.formatLine(line);

        Json::Value segJson;
        segJson.resize(0);
        for (auto&& seg : segs) {
            Json::Value v;
            v["offset"] = seg.offset;
            v["length"] = seg.length;
            segJson.append(v);
        }
        result.append(segJson);
    }

    auto reply = ack(msg, ReplyState::Ok);
    reply["segs"] = result;

    send(reply);

    return Promise::resolved(true);
}

ImplCmdHandler(initRegister) {
    auto myDir = decodeJsonStr(msg["mydir"]);
    auto uid = msg["uid"].asString();
    mRegister.init(myDir, uid);

    auto reply = ack(msg, ReplyState::Ok);
    reply["rstate"] = mRegister.getState();
    reply["left"] = mRegister.getLeftSeconds();

    send(reply);

    return Promise::resolved(true);
}

ImplCmdHandler(doRegister) {
    auto orderId = msg["orderId"].asString();
    auto [ok, info] = mRegister.doRegister(orderId);
    
    auto reply = ack(msg, ReplyState::Ok);
    reply["ok"] = ok;
    reply["info"] = info;

    send(reply);

    return Promise::resolved(true);
}

ImplCmdHandler(exportLog) {
#ifndef OPEN_SOURCE
    auto log = getLog(msg);
    if (!log) {
        send(failedAck(msg, "logId not set"));
        return Promise::resolved(false);
    }

    auto path = decodePath(decodeJsonStr(msg["path"]));

    return Promise::from([log, path, this, msg](bool* cancel) {
        ofstream o(path);
        if (!o.is_open()) {
            send(failedAck(msg, "输出文件打开失败"));
            return false;
        }
        
        auto view = log->view();
        while (!*cancel && !view->end()) {
            auto curLine = string(view->current().str());
            o<<curLine<<endl;
            view->next();
        }

        if (*cancel) {
            send(ack(msg, ReplyState::Cancel));
        } else {
            send(ack(msg, ReplyState::Ok));
        }

        o.close();
        return *cancel;
    });
#else
    return Promise::resolved(true);
#endif
}