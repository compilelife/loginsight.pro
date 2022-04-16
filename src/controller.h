#pragma once

#include "def.h"
#include <event2/buffer.h>
#include <event2/event.h>
#include "json/json.h"
#include "logtree.h"
#include "promisex.h"

class Controller;
typedef Json::Value(Controller::*CmdHandler)(Json::Value&);
extern unordered_map<string,CmdHandler> gCmdHandlers;

struct RegisterHandler {
    RegisterHandler(const char* name, CmdHandler handler) {
        gCmdHandlers[name] = handler;
    }
};

#define DeclarCmdHandler(name)\
Json::Value name##Handler(Json::Value& msg);\
RegisterHandler name##Register{#name, &Controller::name##Handler};

#define ImplCmdHandler(name)\
Json::Value Controller::name##Handler(Json::Value& msg)

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
    void handleCmd(Json::Value& msg);
    DeclarCmdHandler(openFile);
    DeclarCmdHandler(queryPromise);
    DeclarCmdHandler(getRange);

private:
    Json::Value ack(Json::Value& msg, bool success);
    Json::Value promiseAck(Json::Value& msg);
    Json::Value failedAck(Json::Value& msg, string why);
    void send(Json::Value& msg);
};