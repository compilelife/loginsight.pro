#include "register.h"
#include "gtest/gtest.h"
#include <filesystem>
#include <chrono>
#include <fstream>
#include "stdout.h"

#define MOCK_UID "abc"
#define MOCK_HOME "."

using namespace std::filesystem;
using namespace std::chrono;

#define LICENSE path(MOCK_HOME).append(".license")

void cleanup() {
    remove(LICENSE);
}


TEST(Register, noLicense) {
    Register r;
    r.init(MOCK_HOME, MOCK_UID);
    ASSERT_EQ(RegisterState::eTry, r.getState());
    cleanup();
}

void makeTryLicense(hours left) {
    auto passed = seconds(TRY_LIMIT) - left;
    auto fromTimePoint = system_clock::now() - passed;
    auto tryfrom = duration_cast<seconds>(fromTimePoint.time_since_epoch()).count();
    auto licenseStr = "{\"tryfrom\":" + to_string(tryfrom) +"}";
    ofstream o(LICENSE);
    o<<licenseStr;
    o.close();
}

TEST(Register, trying) {
    auto left = hours(3);
    Register r;
    makeTryLicense(left);
    
    r.init(MOCK_HOME, MOCK_UID);
    ASSERT_EQ(eTry, r.getState());
    ASSERT_EQ(duration_cast<seconds>(left).count(), r.getLeftSeconds());

    cleanup();
}

TEST(Register, tryEnd) {
    auto left = hours(-1);
    Register r;
    makeTryLicense(left);
    
    r.init(MOCK_HOME, MOCK_UID);
    ASSERT_EQ(eTryEnd, r.getState());
    ASSERT_EQ(0, r.getLeftSeconds());

    cleanup(); 
}

TEST(Register, doRegister) {
    auto allPlatformOrderId = "3097eaf9a9945bcbbfadb58793ebf13f";
    Register r;
    r.init(MOCK_HOME, MOCK_UID);
    
    auto ret = r.doRegister(allPlatformOrderId);
    ASSERT_TRUE(ret.first)<<ret.second;
    ASSERT_EQ(eRegistered, r.getState());

    cleanup();
}

TEST(Register, platformNoMatch) {
#if defined(__linux__)
    auto orderId = "5a151ca0d5083b0df034454dbdaaa7b0";
#elif defined(_WIN32)
    auto orderId = "5a151ca0d5083b0df034454dbdaaa7b0";
#else
    auto orderId = "e0e5a5eefa1ba17f7ac4dc9f1aed19d4";
#endif
    Register r;
    r.init(MOCK_HOME, MOCK_UID);

    auto oldstate = r.getState();

    auto ret = r.doRegister(orderId);
    LOGE("%s", ret.second.c_str());

    ASSERT_FALSE(ret.first);
    ASSERT_EQ(oldstate, r.getState());

    cleanup();
}

TEST(Register, invalidOrderId) {
    auto orderId = "12345678";
    Register r;
    r.init(MOCK_HOME, MOCK_UID);

    auto oldstate = r.getState();

    auto ret = r.doRegister(orderId);
    LOGE("%s", ret.second.c_str());

    ASSERT_FALSE(ret.first);
    ASSERT_EQ(oldstate, r.getState());

    cleanup();
}