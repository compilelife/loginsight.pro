import QtQuick 2.0
import QtQuick.Dialogs 1.3 as Dialogs
import QtQuick.Controls 2.15
import './app.js' as App
import QtQuick.Layouts 1.15
import './util.js' as Util

Dialogs.Dialog {
  title: 'setting'
  standardButtons: Dialogs.StandardButton.Apply | Dialogs.StandardButton.Cancel
  property var settings: ({//FIXME: can not react ?
                            logView: {
                              font: {
                                size: 14,
                                family: 'monospace'
                              },
                              lineSpacing: 5
                            }
                          })

  GroupBox {//logview
    title: 'log view'
    width: parent.width
    RowLayout {
      Button {
        id: fontBtn
        text: `${settings.logView.font.family}  ${settings.logView.font.size}`
        onClicked: fontDlg.visible = true
      }
      SpinBox {
        id: spacingSpin
        from: 0
        to: 20
        value: settings.logView.lineSpacing
        onValueChanged: {
          settings.logView.lineSpacing = value
        }
      }
    }
  }

  Dialogs.FontDialog {
    id: fontDlg
    onVisibleChanged: {
      if (visible) {
        currentFont.family = settings.logView.font.family
        currentFont.pixelSize = settings.logView.font.size
      }
    }

    onAccepted: {
      settings.logView.font.family = font.family
      settings.logView.font.size = font.pixelSize
      fontBtn.text = `${settings.logView.font.family}  ${settings.logView.font.size}`
    }
  }

  Dialogs.MessageDialog {
    id: confirmRelauch
    title: 'set success'
    text: 'set saved, need reluanch'
    onAccepted: {
      NativeHelper.relaunch()
    }
  }

  Component.onCompleted: {
    //load from settings.json
    const s = NativeHelper.readFile(NativeHelper.settingsPath());
    Util.merge(settings, JSON.parse(s))

    fontBtn.text = `${settings.logView.font.family}  ${settings.logView.font.size}`
    spacingSpin.value = settings.logView.lineSpacing
  }

  onApply: {
    NativeHelper.writeToFile(NativeHelper.settingsPath(), JSON.stringify(settings))
    confirmRelauch.open()
  }
}
