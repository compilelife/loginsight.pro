#pragma once

#include "def.h"
#include <event2/buffer.h>
#include <event2/event.h>
#include "json/json.h"

class Controller {
private:
    event* mReadStdin;
    evbuffer* mLineBuf;
public:
    Controller();
    ~Controller();
    void start();
    void stop();

public:
    void readStdin(int fd);

private:
    void handleCmd(Json::Value& msg);
};