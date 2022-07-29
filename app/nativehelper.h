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
    Q_INVOKABLE void clipboardSetText(const QString& txt);
    Q_INVOKABLE QString writeClipboardToTemp();
    Q_INVOKABLE void relaunch();
    Q_INVOKABLE bool writeToFile(QString path, QString txt);
    Q_INVOKABLE QString readFile(QString path);
    Q_INVOKABLE QString settingsPath();
    Q_INVOKABLE QString myDir();
    Q_INVOKABLE QString uniqueId();
public:
    QString getBinDir();
    QString exeNativeName(QString name);
signals:

};

#endif // NATIVEHELPER_H
