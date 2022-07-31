#include "linehighlighter.h"
#include <QDebug>
#include "textcodec.h"

LineHighlighter::LineHighlighter(QSyntaxHighlighter *parent)
    : QSyntaxHighlighter{parent}
{

}

void LineHighlighter::setup(QQuickTextDocument *document)
{
    setDocument(document->textDocument());
}

void LineHighlighter::highlightBlock(const QString &text)
{
    if (text.isEmpty())
        return;

    //segs
    auto n = qMin(segs.size(), segColors.size());
    for (auto i = 0; i < n; i++) {
        auto& item = segs[i];
        auto offset = item["offset"].toUInt();
        auto length = item["length"].toUInt();

        QTextCharFormat fmt;
        fmt.setForeground(QColor(segColors[i]));
        setFormat(offset, length, fmt);
    }

    //highlights
    for(auto&& item : highlights) {
        auto keyword = item["keyword"].toString();
        auto keywordLen = keyword.length();
        auto color = item["color"].toString();
        auto index = -1;

        QTextCharFormat fmt;
        fmt.clearForeground();//TODO：能否在设置背景的时候 ，根据背景色或保留前景色，或自动选择恰当反色？
        fmt.setBackground(QColor(color));

        do {
            index = text.indexOf(keyword, index+1);
            if (index >= 0)
                setFormat(index, keywordLen, fmt);
        }while(index >= 0);
    }

    //search result
    if (!searchResult.empty()) {
        auto offset = searchResult["offset"].toUInt();
        auto length = searchResult["len"].toUInt();

        //因为返回结果是基于原始编码的，所以不能直接设置给fmt
        //需要将原始编码的字符数组位置映射到text上
        auto logBytes = TextCodec::instance().toLog(text);
        auto keyword = TextCodec::instance().toVisual(logBytes.mid(offset, length));
        auto keywordLength = keyword.length();
        auto suffix = TextCodec::instance().toVisual(logBytes.mid(offset));
        auto index = text.lastIndexOf(suffix);

        QTextCharFormat fmt;
        fmt.clearBackground();
        fmt.setBackground(QColor(0,200,200));
        setFormat(index, keywordLength, fmt);
    }
}



