#include "gtest/gtest.h"
#include "eventloop.h"
#include "controller.h"
#include <thread>
#include <chrono>
#include "stdout.h"
#include <json/json.h>
#include <event2/thread.h>

using namespace std;
using namespace std::chrono_literals;

class NoLimitController : public Controller {
protected:
    bool canUsePro() {//测试阶段无需注册信息
        return true;
    }
     bool enableBase64() {
        return false;
     }
};

class ControllerTest: public testing::Test {
protected:
    unique_ptr<Controller> controller;
    thread thd;
public:
    static void SetUpTestSuite() {
    }
    static void TearDownTestSuite() {
    }
public:
    void SetUp() override {
        thd = thread([]{EventLoop::instance().start();});
        controller.reset(new NoLimitController);
        controller->start();
    }

    void TearDown() override {
        controller->stop();
        controller.reset();
        EventLoop::instance().stop();
        thd.join();
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

#define ASSERT_REPLY(reply, state) ASSERT_EQ(state, reply["state"].asInt())

TEST_F(ControllerTest, invalidCmd) {
    controller->handleLine("abcdefg");
    bool success;
    lastReply(&success);
    ASSERT_FALSE(success);

    controller->handleLine(R"({"cmd":"loginsight", "id":"ui-1"})");
    ASSERT_FALSE(lastReply()["success"].asBool());
}

TEST_F(ControllerTest, boot) {
    controller->handleLine(R"({"cmd":"openFile","id":"ui-1","path":"./sample.log"})");
    ASSERT_REPLY(lastReply(), ReplyState::Future);

    controller->handleLine(R"({"cmd":"queryPromise", "id":"ui-2", "pid": 1})");
    ASSERT_REPLY(lastReply(), ReplyState::Ok);

    this_thread::sleep_for(5s);//等待promise解析完成
    {
        auto reply = lastReply();
        ASSERT_REPLY(reply, ReplyState::Ok);
        ASSERT_STREQ("ui-1", reply["id"].asCString());
        ASSERT_EQ(Range(0, 534074), Range(reply["range"]));
    }

    controller->handleLine(R"({"cmd":"queryPromise", "id":"ui-3", "pid": 1})");
    {
        auto reply = lastReply();
        ASSERT_EQ(100, reply["progress"].asInt());
    }

    controller->handleLine(R"({"cmd": "getLines", "id": "ui-4", "range": {"begin": 0, "end": 1}, "logId":1})");
    this_thread::sleep_for(500ms);
    {
        auto reply = lastReply();
        ASSERT_REPLY(reply, ReplyState::Ok);
        ASSERT_EQ(2, reply["lines"].size());
    }

    controller->handleLine(R"({"cmd": "closeLog", "id": "ui-5", "logId": 1})");
    this_thread::sleep_for(100ms);
    ASSERT_REPLY(lastReply(), ReplyState::Ok);
}

TEST_F(ControllerTest, find) {
    controller->handleLine(R"({"cmd":"openFile","id":"ui-1","path":"sample.log"})");
    this_thread::sleep_for(5s);

    controller->handleLine(R"({"id":"ui-10","cmd":"search","logId":1,"fromLine":4768,"fromChar":28,"pattern":"IJKMEDIA","caseSense":false,"reverse":true,"regex":false})");
    this_thread::sleep_for(500ms);

    auto reply = lastReply();
    ASSERT_TRUE(reply["found"].asBool());
    ASSERT_EQ(4697, reply["index"].asUInt64());
    ASSERT_EQ(21, reply["offset"].asUInt());
    ASSERT_EQ(8, reply["len"].asUInt());
}