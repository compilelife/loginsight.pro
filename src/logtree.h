#pragma once

#include "log.h"
#include "def.h"
#include <unordered_map>

using LogId = size_t;
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
    LogId addLog(const shared_ptr<ILog>& parent, shared_ptr<ILog>&& child);
    shared_ptr<ILog> getLog(LogId id);
    bool isRoot(shared_ptr<ILog>& log);
    void delLog(LogId id);

private:
    unique_ptr<Node> mRootNode;
    LogId mIdGen{0};
};