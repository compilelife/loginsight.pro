#pragma once

#include "def.h"
#include <event2/buffer.h>
#include <event2/event.h>
#include "json/json.h"
#include "logtree.h"
#include "promisex.h"
#include "pingtask.h"
#include <optional>
#include "calculation.h"
#include "linesegment.h"
#include "eventloop.h"
#include "register.h"

using JsonMsg = const Json::Value&;

class Controller;
//不要使用promise::value，因为每个任务返回的值不同
typedef shared_ptr<Promise>(Controller::*CmdHandler)(JsonMsg);
struct CmdHandlerWrap {
    EventType type;
    CmdHandler handler;
};
extern unordered_map<string,CmdHandlerWrap> gCmdHandlers;

struct RegisterHandler {
    RegisterHandler(const char* name, CmdHandler handler, EventType type) {
        gCmdHandlers[name] = {type, handler};
    }
};

#define DeclarCmdHandler(name, type) \
shared_ptr<Promise> name##Handler(JsonMsg msg);\
RegisterHandler name##Register{#name, &Controller::name##Handler, type};

#define ImplCmdHandler(name)\
shared_ptr<Promise> Controller::name##Handler(JsonMsg msg)

enum ReplyState {
    Ok = 0,
    Fail,
    Future,
    Cancel
};

class Controller {
private:
    event* mReadStdin;
    evbuffer* mLineBuf;
    LogTree mLogTree;
    Json::CharReaderBuilder mRPCReaderBuilder;
    LineSegment mLineSegment;
    size_t mIdGen{0};
    shared_ptr<PingTask> mWatchDisconnectTask;
    shared_ptr<PingTask> mWatchRangeTask;

    size_t mPromiseIdGen{0};
    map<size_t, shared_ptr<Promise>> mTaskPromises;

public:
    Controller();
    ~Controller();
    void start();
    void stop();

public:
    void readStdin(int fd);
    void mockInput(string_view s);

private:
    void handleCmd(JsonMsg msg);
    DeclarCmdHandler(openFile, EventType::Write);
    DeclarCmdHandler(openProcess, EventType::Write);
    DeclarCmdHandler(openMultiFile, EventType::Write);
    DeclarCmdHandler(queryPromise, EventType::Immediate);
    DeclarCmdHandler(cancelPromise, EventType::Immediate);
    DeclarCmdHandler(getRange, EventType::Read);
    DeclarCmdHandler(getLines, EventType::Read);
    DeclarCmdHandler(filter, EventType::Read);
    DeclarCmdHandler(search, EventType::Read);
    DeclarCmdHandler(closeLog, EventType::Write);
    DeclarCmdHandler(listFiles, EventType::Immediate);
    DeclarCmdHandler(mapLine, EventType::Read);
    DeclarCmdHandler(setLineSegment, EventType::Write);
    DeclarCmdHandler(syncLogs, EventType::Write);
    DeclarCmdHandler(testSyntax, EventType::Immediate);
    DeclarCmdHandler(initRegister, EventType::Immediate);
    DeclarCmdHandler(doRegister, EventType::Immediate);

private:
    Json::Value ack(JsonMsg msg, ReplyState state);
    Json::Value failedAck(JsonMsg msg, string why);
    void send(JsonMsg msg);
    shared_ptr<ILog> getLog(JsonMsg msg);
    bool handleCancelledPromise(shared_ptr<Promise>& p, JsonMsg msg);
    bool handleLine();
    struct FindLogRet {
        LineRef line;
        FindRet extra;
    };
    shared_ptr<Promise> find(shared_ptr<ILog> log, 
                            FindFunction f, 
                            LogLineI fromLine, 
                            LogCharI fromChar, 
                            bool reverse);
    Json::Value onRootLogReady(JsonMsg msg, shared_ptr<IClosableLog> log);
    void onRootLogFinalize();
    string nextId();
    Json::Value prepareMsg(string_view cmd);
    Register mRegister;
};