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

class ControllerTest: public testing::Test {
protected:
    unique_ptr<Controller> controller;
public:
    static void SetUpTestSuite() {
#ifdef EVTHREAD_USE_PTHREADS_IMPLEMENTED
    evthread_use_pthreads();
#elif EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED
    evthread_use_windows_threads();
#endif
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

#define ASSERT_REPLY(reply, state) ASSERT_EQ(state, reply["state"].asInt())

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
    ASSERT_REPLY(reply, ReplyState::Future);
    ASSERT_EQ("ui-1", reply["id"].asString());
}

TEST_F(ControllerTest, boot) {
    controller->mockInput(R"({"cmd":"openFile","id":"ui-1","path":"./sample.log"})");
    ASSERT_REPLY(lastReply(), ReplyState::Future);

    controller->mockInput(R"({"cmd":"queryPromise", "id":"ui-2"})");
    ASSERT_REPLY(lastReply(), ReplyState::Ok);

    this_thread::sleep_for(5s);//等待promise解析完成
    {
        auto reply = lastReply();
        ASSERT_REPLY(reply, ReplyState::Ok);
        ASSERT_STREQ("ui-1", reply["id"].asCString());
        ASSERT_EQ(Range(0, 534074), Range(reply["range"]));
    }

    controller->mockInput(R"({"cmd":"queryPromise", "id":"ui-3"})");
    {
        auto reply = lastReply();
        ASSERT_EQ(100, reply["progress"].asInt());
    }

    controller->mockInput(R"({"cmd": "getLines", "id": "ui-4", "range": {"begin": 0, "end": 1}, "logId":1})");
    this_thread::sleep_for(500ms);
    {
        auto reply = lastReply();
        ASSERT_REPLY(reply, ReplyState::Ok);
        ASSERT_EQ(2, reply["lines"].size());
    }
}