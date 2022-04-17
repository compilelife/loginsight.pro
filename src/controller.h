#pragma once

#include "def.h"
#include <event2/buffer.h>
#include <event2/event.h>
#include "json/json.h"
#include "logtree.h"
#include "promisex.h"
#include "pingtask.h"
#include <optional>

using JsonMsg = const Json::Value&;

class Controller;
typedef Json::Value(Controller::*CmdHandler)(JsonMsg);
struct CmdHandlerWrap {
    bool waitBarrier;
    CmdHandler handler;
};
extern unordered_map<string,CmdHandlerWrap> gCmdHandlers;

struct RegisterHandler {
    RegisterHandler(const char* name, CmdHandler handler, bool waitBarrier=true) {
        gCmdHandlers[name] = {waitBarrier, handler};
    }
};

#define DeclarCmdHandler(name) DeclarCmdHandler2(name, true)

#define DeclarCmdHandler2(name, waitBarrier)\
Json::Value name##Handler(JsonMsg msg);\
RegisterHandler name##Register{#name, &Controller::name##Handler, waitBarrier};

#define ImplCmdHandler(name)\
Json::Value Controller::name##Handler(JsonMsg msg)

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
    shared_ptr<Promise> mBarrierPromise;
    Json::FastWriter mRPCWriter;
    Json::CharReaderBuilder mRPCReaderBuilder;

public:
    Controller();
    ~Controller();
    void start();
    void stop();

public:
    void readStdin(int fd);
    void mockInput(string_view s);

private:
    bool handleLine();

private:
    void handleCmd(JsonMsg msg);
    DeclarCmdHandler(openFile);
    DeclarCmdHandler2(queryPromise, false);
    DeclarCmdHandler(getRange);
    DeclarCmdHandler(getLines);

private:
    Json::Value ack(JsonMsg msg, ReplyState state);
    Json::Value failedAck(JsonMsg msg, string why);
    void send(JsonMsg msg);
    shared_ptr<ILog> getLog(JsonMsg msg);
    bool handleCancelledPromise(shared_ptr<Promise>& p, JsonMsg msg);
};