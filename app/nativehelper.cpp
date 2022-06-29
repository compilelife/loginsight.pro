#include "nativehelper.h"
#include <QApplication>
#include <QClipboard>
#include <QProcess>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QDebug>

NativeHelper::NativeHelper(QObject *parent)
    : QObject{parent}
{

}

void NativeHelper::clipboardSetImage(const QImage img)
{
    qApp->clipboard()->setImage(img);
}

QString NativeHelper::writeClipboardToTemp()
{
    auto content = qApp->clipboard()->text();
    if (content.isEmpty()) {
        qDebug()<<"clipboard text is empty";
        return "";
    }

    auto path = QDir::temp().filePath("loginsight-clipboard.txt");

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qDebug()<<"failed to open "<<path;
        return "";
    }

    f.write(content.toLocal8Bit());
    f.close();

    return path;
}

void NativeHelper::relaunch()
{
    //FIXME: with next line uncommented, old window won't exit
    //QProcess::startDetached(qApp->applicationFilePath(), QStringList());
    qApp->quit();
}

bool NativeHelper::writeToFile(QString path, QString txt)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(txt.toLocal8Bit());
        f.close();
        return true;
    }
    return false;
}

QString NativeHelper::readFile(QString path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        auto s = QString::fromLocal8Bit(f.readAll());
        f.close();
        return s;
    }
    return "";
}

QString NativeHelper::settingsPath()
{
    return QDir::home().filePath(".loginsight/settings.json");
}
