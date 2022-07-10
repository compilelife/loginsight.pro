#include "linehighlighter.h"
#include <QDebug>

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
        QTextCharFormat fmt;
        fmt.clearBackground();
        fmt.setBackground(QColor(0,200,200));
        setFormat(offset, length, fmt);
    }
}



