#include "register.h"
#include "stdout.h"
#include "curl/curl.h"
#include "json/json.h"
#include <chrono>
#include "stdout.h"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace std::chrono;
using namespace std::filesystem;

static string getPlatform() {
#if defined(__linux__)
    return "Linux";
#elif defined(_WIN32)
    return "Windows";
#else
    return "Mac";
#endif
}

RegisterState Register::getState() {
    return mState;
}

int64_t Register::getLeftSeconds() {
    return mLeftSeconds;
}

void Register::writeLicense(string token) {
    auto licensePath = path(mMyDir).append(".license").native();
    ofstream f(licensePath);
    f<<token;
    f.close();
}

string Register::readLicense() {
    auto licensePath = path(mMyDir).append(".license").native();
    ifstream f(licensePath);
    if (!f.is_open())
        return "";

    string ret;
    f>>ret;
    f.close();

    return ret;
}

static size_t writeToString(void* content, size_t size, size_t count, void* userData) {
    ((std::string*)userData)->append((char*)content, 0, size* count);
    return size* count;
}

/**
 * @brief 注册成功返回token，上层应保存该token；保存失败返回出错信息 ，上层应对用户展示该信息
 * 
 * @param uniqueId 机器唯一码
 * @param orderid 订单号
 */
pair<bool, string> Register::doRegister(string orderid) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, "curl init failed"};
    }

    string arg = "orderid="+orderid+"&machineid="+mUniqueId+"&platform="+getPlatform();
    string url = "https://www.loginsight.top/api/register?"+arg;

    string body;
    char err[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err);

    pair<bool, string> ret(true, "");
    LOGI("curl: %s", url.c_str());
    auto curlRet = curl_easy_perform(curl);
    if(curlRet != CURLE_OK) {
        LOGE("curl failed (%s): %s", curl_easy_strerror(curlRet), err);
        ret = {false, "perform curl failed"};
    }

    curl_easy_cleanup(curl);

    if (ret.first) {
        Json::CharReaderBuilder readerBuilder;
        Json::Value root;
        Json::String errs;
        stringstream ss(body);
        auto parseRet = Json::parseFromStream(readerBuilder, ss, &root, &errs);
        if (parseRet) {
            auto success = root.isMember("success") ? root["success"].asBool() : true;
            if (success)  {
                ret = {true, body};
                root.removeMember("success");
                root.removeMember("reason");
                Json::FastWriter writer;
                writeLicense(writer.write(root));
                mState = eRegistered;
            }
            else {
                ret = {false, root["reason"].asString()};
            }
        } else {
            ret = {false, "register server error"};
        }
    }

    return ret;
}

static string xordecode(const Json::Value& arr, int key) {
    uint8_t keys[4];
    for (size_t i = 0; i < 4; i++) {
        keys[i] = key&0xFF;
        key >>= 8;
    }

    string ret;
    ret.resize(arr.size());
    auto decoded = ret.data();
    int j = 0;
    for (size_t i = 0; i < arr.size(); i++) {
        decoded[i] = arr[(Json::ArrayIndex)i].asInt() ^ keys[j++];
        if (j >= 4)
            j = 0;
    }
    
    return ret;    
}

void Register::init(string mydir, string uniqueId) {
    mUniqueId = uniqueId;
    mMyDir = mydir;
#ifdef __APPLE__
    mState = eRegistered;
#else
    auto token = readLicense();

    if (token.empty()) {
        mState = eTry;
        mLeftSeconds = TRY_LIMIT;

        auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        Json::Value root;
        root["tryfrom"]=now;
        Json::FastWriter writer;
        writeLicense(writer.write(root));

        return;
    }

    Json::CharReaderBuilder readerBuilder;
    Json::Value root;
    Json::String errs;
    stringstream ss(token);
    auto parseRet = Json::parseFromStream(readerBuilder, ss, &root, &errs);
    if (parseRet) {
        if (root.isMember("ordertime")) {
            auto ordertime = root["ordertime"].asInt();
            auto machineId = xordecode(root["mid"], ordertime);
            auto platform = xordecode(root["plf"], ordertime);
            if (machineId != uniqueId) {
                LOGE("你的授权文件不是在该设备上注册的");
                return;
            }
            if (platform != getPlatform()) {
                LOGE("你的授权文件不能用于该操作系统，请购买对应操作系统的授权");
                return;
            }
            mState = RegisterState::eRegistered;
        } else {
            if (root.isMember("tryfrom")) {
                auto tryfrom = seconds(root["tryfrom"].asInt64());
                auto passed = system_clock::now() - time_point<system_clock>(tryfrom);
                auto left = seconds(TRY_LIMIT) - duration_cast<seconds>(passed);
                mLeftSeconds = left.count();
                if (mLeftSeconds < 0) {
                    mState = RegisterState::eTryEnd;
                    mLeftSeconds = 0;
                } else {
                    mState = RegisterState::eTry;
                }
            }
        }
    } else {
        LOGE("load register info failed: %s", errs.c_str());
    }
#endif
}