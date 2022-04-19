#include "logtree.h"

LogId LogTree::setRoot(shared_ptr<IClosableLog>&& root) {
    auto id = ++mIdGen;
    mRootNode.reset(new Node{
        id,
        root,
        {}
    });
    return id;
}

bool travelTree(Node* root, const function<bool(Node* node)>& visit) {
    if (visit(root))
        return true;
    for (auto &&it : root->children) {
        if (travelTree(it.second.get(), visit)) 
            return true;
    }
    return false;    
}

LogId LogTree::addLog(shared_ptr<ILog>& parent, shared_ptr<ILog>&& child) {
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