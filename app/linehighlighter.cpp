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
        fmt.clearForeground();//FIXME: keep foreground if possible
        fmt.setBackground(QColor(color));

        do {
            index = text.indexOf(keyword, index+1);
            if (index >= 0)
                setFormat(index, keywordLen, fmt);
        }while(index >= 0);
    }
}

