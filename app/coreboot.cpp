#include "coreboot.h"
#include <QDebug>
#include <QTcpServer>
#include "nativehelper.h"

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

    connect(&mProcess, &QProcess::readyRead, [this]{
        if (mProcess.canReadLine()) {
            auto line = mProcess.readLine();
            newLine(line);
        }
    });
}

void CoreBoot::startLocal() {
    NativeHelper native;
    mProcess.setProgram(native.getBinDir()+"/"+native.exeNativeName("core"));
    mProcess.start();
}

void CoreBoot::stop() {
    mProcess.close();
}

void CoreBoot::send(QString s) {
    mProcess.write((s+'\n').toLocal8Bit());
}
