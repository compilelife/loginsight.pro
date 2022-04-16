#include "gtest/gtest.h"
#include "eventloop.h"
#include "controller.h"
#include <thread>
#include <chrono>
#include "stdout.h"
#include <json/json.h>

using namespace std;
using namespace std::chrono_literals;

class ControllerTest: public testing::Test {
protected:
    unique_ptr<Controller> controller;
public:
    static void SetUpTestSuite() {
        thread([]{EventLoop::instance().start();}).detach();
    }
    static void TearDownTestSuite() {
        EventLoop::instance().stop();
    }
public:
    void SetUp() override {
        controller.reset(new Controller);
        controller->start();
    }

    void TearDown() override {
        controller->stop();
        controller.reset();
    }
};

Json::Value lastReply(bool* success = nullptr) {
    Json::CharReaderBuilder builder;
    stringstream ss(StdOut::instance().lastLine());
    Json::Value root;
    auto ret = Json::parseFromStream(builder, ss, &root, nullptr);
    if (success != nullptr)
        *success = ret;
    return root;
}

#define ASSERT_REPLY_SUCCESS() ASSERT_TRUE(lastReply()["success"].asBool())

TEST_F(ControllerTest, invalidCmd) {
    controller->mockInput("abcdefg");
    bool success;
    lastReply(&success);
    ASSERT_FALSE(success);

    controller->mockInput(R"({"cmd":"loginsight", "id":"ui-1"})");
    ASSERT_FALSE(lastReply()["success"].asBool());
}

TEST_F(ControllerTest, openFile) {
    controller->mockInput(R"({"cmd":"openFile","id":"ui-1","path":"./sample.log"})");
    auto reply = lastReply();
    ASSERT_TRUE(reply["success"].asBool());
    ASSERT_EQ("ui-1", reply["id"].asString());
    ASSERT_TRUE(reply["promise"].asBool());
}

TEST_F(ControllerTest, boot) {
    controller->mockInput(R"({"cmd":"openFile","id":"ui-1","path":"./sample.log"})");
    ASSERT_REPLY_SUCCESS();

    controller->mockInput(R"({"cmd":"queryPromise", "id":"ui-2"})");
    ASSERT_REPLY_SUCCESS();

    this_thread::sleep_for(5s);//等待promise解析完成
    controller->mockInput(R"({"cmd":"queryPromise", "id":"ui-3"})");
    {
        auto reply = lastReply();
        ASSERT_EQ(100, reply["progress"].asInt());
    }

    controller->mockInput(R"({"cmd":"getRange", "id": "ui-4", "logId": 1})");
    {
        auto reply = lastReply();
        ASSERT_EQ(0, reply["range"]["begin"].asInt());
        ASSERT_EQ(534074, reply["range"]["end"].asInt());
    }
}