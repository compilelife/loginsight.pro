#include "logtree.h"
#include "stdout.h"

LogId LogTree::setRoot(shared_ptr<IClosableLog> root) {
    if (mRootNode) {
        auto rootLog = dynamic_cast<IClosableLog*>(mRootNode->log.get());
        rootLog->close();
    }
    
    auto id = ++mIdGen;
    mRootNode.reset(new Node{
        id,
        root,
        {}
    });
    return id;
}

//visit返回true立即停止继续遍历树
bool travelTree(Node* root, const function<bool(Node* node)>& visit) {
    if (visit(root))
        return true;
    for (auto &&it : root->children) {
        if (travelTree(it.second.get(), visit)) 
            return true;
    }
    return false;    
}

bool LogTree::travel(const function<bool(Node* node)>& visit) {
    return travelTree(mRootNode.get(), visit);
}

LogId LogTree::addLog(const shared_ptr<ILog>& parent, shared_ptr<ILog>&& child) {
    LogId id = ++mIdGen;
    auto ret = travel([&, this](Node* node){
        if (node->log == parent) {
            node->children[id].reset(new Node{
                id,
                child,
                {}
            });
            return true;
        }

        return false;
    });
    return ret ? id : INVALID_LOG_ID;
}

shared_ptr<ILog> LogTree::getLog(LogId id) {
    shared_ptr<ILog> ret;
    travel([&](Node* node){
        if (node->id != id)
            return false;
        
        ret = node->log;
        return true;
    });

    return ret;
}

void LogTree::delLog(LogId id) {
    if (id == mRootNode->id) {
        ((IClosableLog*)mRootNode->log.get())->close();
    }

    travel([id, this](Node* node){
        const auto it = node->children.find(id);
        if (it == node->children.end())
            return false;
        
        auto& child = (*it).second;
        LOGI("del log: %u", child->id);
        node->children.erase(it);
        return true;
    });
}

map<LogId, LogLineI> LogTree::mapLine(LogLineI srcLine) {
    map<LogId, LogLineI> ret;
    auto rootNodeId = mRootNode->id;

    ret[mRootNode->id] = srcLine;

    travel([&ret, srcLine](Node* node){
        auto line = node->log->fromSource(srcLine);
        if (line != InvalidLogLine)
            ret[node->id] = line;
        return false;
    });

    return ret;
}

shared_ptr<ILog> LogTree::root() {
    return mRootNode->log;
}