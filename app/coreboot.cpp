#include "coreboot.h"
#include <QDebug>

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
    mProcess.setProgram("/home/chenyong/my/loginsight/core/third/websocketd.linux");
    QStringList args;
    args<<"-port"<<"8080";
    args<<"/home/chenyong/my/loginsight/core/build/linux/x86_64/debug/core";
    mProcess.setArguments(args);
    mProcess.start();

    mUrl = "ws://localhost:8080";
}

void CoreBoot::stop() {
    mProcess.close();
}
