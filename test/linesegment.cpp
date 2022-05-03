#include "linesegment.h"
#include "gtest/gtest.h"
#include <iostream>

TEST(LineSegment, androidLog) {
    string_view s(R"(12-23 14:19:59.359 I/chromium( 3941): [INFO:CONSOLE(36)] "[KMSplit:kmtvgame:28626] [OpenJS] recv data: {"cmdid":"keepAlive","sessionid":1000,"tvgameid":"dicegame"}", source: http://192.168.96.40:81/KMGame/public/split/dist/bundle.js?a4f58a91a09970ad7268 (36))");
    regex r(R"((\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}) (\w)/(\w+)\(( *\d+)\))");

    LineSegment formater;
    formater.setPattern(r);
    formater.setSegments({
        {SegType::Date, "TimeStamp"},
        {SegType::LogLevel, "LogLevel"},
        {SegType::Str, "ProcessName"},
        {SegType::Num, "PID"}
    });

    auto segs = formater.formatLine(string(s));
    auto debug = formater.makeDebugPrint(s, segs);

    cout<<debug;
}