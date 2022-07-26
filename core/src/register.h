#pragma once

#include "def.h"
#include <string>
#include <utility>

#define TRY_LIMIT 259200 //3天

enum RegisterState {
    eOpenSource,
    eTry,
    eTryEnd,
    eRegistered
};

class Register {
public:
    void init(string mydir, string uniqueId);
    RegisterState getState();
    int64_t getLeftSeconds();
    pair<bool, string> doRegister(string orderid);
private:
    void writeLicense(string token);
    string readLicense();
private:
#ifdef OPEN_SOURCE
    RegisterState mState{eOpenSource};
#else
    RegisterState mState{eTryEnd};
#endif
    string mMyDir;
    string mUniqueId;
    int64_t mLeftSeconds{0};
};