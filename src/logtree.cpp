#include "logtree.h"

LogId LogTree::setRoot(shared_ptr<ILog>&& root) {
    auto id = ++mIdGen;
    mRootNode.reset(new Node{
        ++mIdGen,
        root,
        {}
    });
    return id;
}

bool travelTree(Node* root, const function<bool(Node* node)>& visit) {
    auto last = root;
    while (last) {
        if (visit(last))
            return true;
        for (auto &&it : last->children) {
            if (travelTree(it.second.get(), visit)) 
                return true;
        }
    }
    return false;    
}

LogId LogTree::addNode(shared_ptr<ILog>& parent, shared_ptr<ILog>&& child) {
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