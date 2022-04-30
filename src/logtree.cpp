#include "logtree.h"

LogId LogTree::setRoot(shared_ptr<IClosableLog> root) {
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

LogId LogTree::addLog(const shared_ptr<ILog>& parent, shared_ptr<ILog>&& child) {
    LogId id = ++mIdGen;
    auto ret = travelTree(mRootNode.get(), [&, this](Node* node){
        if (node->log == parent) {
            node->children[id].reset(new Node{
                id,
                child,
                {}
            });
        }

        return false;
    });
    return ret ? id : INVALID_LOG_ID;
}

shared_ptr<ILog> LogTree::getLog(LogId id) {
    shared_ptr<ILog> ret;
    travelTree(mRootNode.get(), [&](Node* node){
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

    travelTree(mRootNode.get(), [id, this](Node* node){
        const auto it = node->children.find(id);
        if (it == node->children.end())
            return false;
        
        auto& child = (*it).second;
        child->children.clear();
        node->children.erase(it);
    });
}

map<LogId, LogLineI> LogTree::mapLine(LogLineI srcLine) {
    map<LogId, LogLineI> ret;
    auto rootNodeId = mRootNode->id;

    ret[mRootNode->id] = srcLine;

    travelTree(mRootNode.get(), [&ret, srcLine](Node* node){
        ret[node->id] = node->log->fromSource(srcLine);
        return false;
    });

    return ret;
}