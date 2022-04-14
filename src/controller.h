#pragma once

#include "def.h"
#include <event2/buffer.h>
#include <event2/event.h>
#include "json/json.h"
#include "logtree.h"

class Controller {
private:
    event* mReadStdin;
    evbuffer* mLineBuf;
    LogTree mLogTree;
public:
    Controller();
    ~Controller();
    void start();
    void stop();

public:
    void readStdin(int fd);

private:
    void handleCmd(Json::Value& msg);
    void handleOpenFile(Json::Value& msg);
    void replyFailMsg(Json::Value& msg, string_view why);
    void replyCancelled(Json::Value& msg);
};