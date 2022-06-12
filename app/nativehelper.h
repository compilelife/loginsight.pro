#ifndef NATIVEHELPER_H
#define NATIVEHELPER_H

#include <QObject>
#include <QImage>

class NativeHelper : public QObject
{
    Q_OBJECT
public:
    explicit NativeHelper(QObject *parent = nullptr);
    Q_INVOKABLE void clipboardSetImage(const QImage img);
signals:

};

#endif // NATIVEHELPER_H
