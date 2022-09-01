#pragma once

#include "log.h"
#include "def.h"
#include <unordered_map>
#include "json/value.h"

using LogId = Json::UInt;//只是为了避免 ret["logId"] = logId 时提示ambiguous
#define INVALID_LOG_ID 0

struct Node
{
    LogId id;
    shared_ptr<ILog> log;
    unordered_map<LogId, unique_ptr<Node>> children;
};


class LogTree {
public:
    LogId setRoot(shared_ptr<IClosableLog> root);
    shared_ptr<ILog> root();
    LogId addLog(const shared_ptr<ILog>& parent, shared_ptr<ILog>&& child);
    shared_ptr<ILog> getLog(LogId id);
    bool isRoot(shared_ptr<ILog>& log);
    void delLog(LogId id);
    bool travel(const function<bool(Node* node)>& visit);

public:
    map<LogId, LogLineI> mapLine(LogLineI srcLine);

private:
    unique_ptr<Node> mRootNode;
    LogId mIdGen{0};
};