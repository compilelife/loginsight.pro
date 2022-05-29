#ifndef COREBOOT_H
#define COREBOOT_H

#include <QObject>
#include <QProcess>

class CoreBoot : public QObject
{
    Q_OBJECT
public:
    explicit CoreBoot(QObject *parent = nullptr);

public:
    Q_PROPERTY(QString url MEMBER mUrl);
    Q_INVOKABLE void startLocal();
    Q_INVOKABLE void stop();

signals:
    void stateChanged(bool running);

private:
    QString mUrl;
    QProcess mProcess;
};

#endif // COREBOOT_H
