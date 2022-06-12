#include "nativehelper.h"
#include <QApplication>
#include <QClipboard>

NativeHelper::NativeHelper(QObject *parent)
    : QObject{parent}
{

}

void NativeHelper::clipboardSetImage(const QImage img)
{
    qApp->clipboard()->setImage(img);
}
