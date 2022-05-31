#include "coreboot.h"
#include <QDebug>
#include <QTcpServer>

CoreBoot::CoreBoot(QObject *parent)
    : QObject{parent}
{
    connect(&mProcess, &QProcess::stateChanged, [this](QProcess::ProcessState state){
        if (state == QProcess::NotRunning) {
            emit stateChanged(false);
        } else if (state == QProcess::Running) {
            emit stateChanged(true);
        }
    });

    auto readProcessOutput = [this]{
        qDebug()<<mProcess.readAllStandardOutput();
    };
    connect(&mProcess, &QProcess::readyReadStandardOutput, readProcessOutput);

}

void CoreBoot::startLocal() {
    auto port = getIdlePort();
    if (port < 0) {
        qFatal("no idle port");
        return;
    }

    mProcess.setProgram("/home/chenyong/my/loginsight/core/third/websocketd.linux");
    QStringList args;
    args<<"-port"<<QString::number(port);
    args<<"/home/chenyong/my/loginsight/core/build/linux/x86_64/debug/core";
    mProcess.setArguments(args);
    mProcess.start();

    mUrl = "ws://localhost:"+QString::number(port);
}

void CoreBoot::stop() {
    mProcess.close();
}

int CoreBoot::getIdlePort() {
    QTcpServer server;
    if (server.listen()) {
        auto ret = server.serverPort();
        server.close();
        return ret;
    }
    return -1;
}
