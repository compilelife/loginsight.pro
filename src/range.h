#pragma once

#include <cstdint>
#include <cmath>
#include "json/json.h"

using namespace std;

/**
 * @brief begin和end分别指向开始与结束，都是被包含，即： [begin, end]
 * 
 */
struct Range {
    uint64_t begin {1};
    uint64_t end {0};

    Range(uint64_t b, uint64_t e)
        :begin(b), end(e) {
    }

    Range(){}

    Range(const Json::Value& v) {
        begin = v["begin"].asUInt64();
        end = v["end"].asUInt64();
    }

    void writeTo(Json::Value& v) {
        v["begin"] = begin;
        v["end"] = end;
    }

    bool operator==(const Range& other) const {
        return begin == other.begin && end == other.end;
    }

    bool valid() {
        return end >= begin;
    }

    bool overlapped(const Range& other) {
        //https://www.cnblogs.com/liuwt365/p/7222549.html
        return max(other.begin, begin) <= min(other.end, end);
    }

    bool operator == (const Range& other) {
        return begin == other.begin && end == other.end;
    }

    uint64_t len() {
        return end-begin+1;
    }
};