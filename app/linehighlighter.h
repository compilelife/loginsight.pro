#ifndef LINEHIGHLIGHTER_H
#define LINEHIGHLIGHTER_H

#include <QObject>
#include <QSyntaxHighlighter>
#include <QQuickTextDocument>

class LineHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit LineHighlighter(QSyntaxHighlighter *parent = nullptr);

    Q_INVOKABLE void setup(QQuickTextDocument* document);

    Q_PROPERTY(QVector<QString> segColors MEMBER segColors)
    Q_PROPERTY(QVector<QVariantMap> highlights MEMBER highlights)
    Q_PROPERTY(QVector<QVariantMap> segs MEMBER segs)
    Q_PROPERTY(QVariantMap searchResult MEMBER searchResult)

private:
    QVector<QVariantMap> segs;
    QVector<QString> segColors;
    QVector<QVariantMap> highlights;
    QVariantMap searchResult;

protected:
    void highlightBlock(const QString &text) override;

};

#endif // LINEHIGHLIGHTER_H
