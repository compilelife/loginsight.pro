import QtQuick 2.15
import QtQuick.Window 2.15
import QtWebSockets 1.15
import QtQuick.Controls 2.15 as QC2
import QtQuick.Layouts 1.15
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Dialogs 1.3
import './app.js' as App

ApplicationWindow {
  width: 1000
  height: 700

  visible: true
  title: qsTr("LogInsight")

  property alias toast: _toast

  Actions{
    id: actions
  }

  menuBar: MenuBar {
    id: menubar
    Menu {
      title: "File"
      MenuItem {action: actions.open}
      MenuItem {action: actions.openProcess}
      MenuItem {action: actions.openClipboard}
      MenuItem {action: actions.close}
    }
    Menu {
      title: "Insight"
      MenuItem {action: actions.filter}
      MenuItem {action: actions.search}
      MenuSeparator{}
      MenuItem {action: actions.goTo}
      MenuItem {action: actions.goBack}
      MenuItem {action: actions.goForward}
      MenuSeparator{}
      MenuItem {action: actions.setSyntax}
    }
    Menu {
      title: "TimeLine"
      MenuItem {action: actions.clearTimeLine}
      MenuItem {action: actions.shotTimeLine}
    }
    Menu {
      title: "others"
      MenuItem {action: actions.settings}
    }
  }

  toolBar: ToolBar {
    height: toolbtns.height
    RowLayout {
      id: toolbtns
      width: parent.width
      QC2.TabBar {
        contentHeight: 30
        currentIndex: -1
        id: tabBar
      }
      Row {
        Layout.alignment: Qt.AlignRight
        ToolButton{action: actions.followLog}
        QC2.ToolSeparator{}
        ToolButton{action: actions.search}
        ToolButton{action: actions.filter}
        ToolButton{action: actions.goTo}
        ToolButton{action: actions.goBack}
        ToolButton{action: actions.goForward}
        QC2.ToolSeparator{}
        ToolButton{action: actions.clearTimeLine}
        ToolButton{action: actions.shotTimeLine}
      }
    }
  }

  StackLayout {
    id: sessions
    currentIndex: tabBar.currentIndex
    anchors.fill: parent
    onCountChanged: {
      handleCurrentItemChanged()
    }
    onCurrentIndexChanged: {
      if (currentSession()) {
        handleCurrentItemChanged()
      }
    }

    function handleCurrentItemChanged() {
      if (currentIndex >= 0) {
        App.setCurrentSession(currentSession())
      } else {
        App.setCurrentSession(null)
        App.setCurrentView(null)
      }
      actions.updateSessionActions(currentIndex >= 0)
      currentSession().setAsCurrent()
    }
  }

  FileDialog {
    id: openDlg
    title: 'please choose log or project file to open'
    folder: shortcuts.home
    onAccepted: {
      const url = openDlg.fileUrl.toString().substring(7) //drop file://
      doOpenFileOrPrj(url)
    }
  }

  Settings {
    id: settings
  }

  Toast {
    id: _toast
    visible: false
  }

  Dialog {
    id: pmDlg
    width: 500
    height: 400
    ProcessManager{
      id: pm
    }
    onAccepted: {
      const args = pm.getUserSelect()
      if (args.process.length === 0) {
        _toast.show('process not specific')
        return
      }

      const process = args.process
      const session = addSession("process")
      //TODO: support set cache
      //TODO: support stderr
      session.coreReady.connect(function () {
        session.openProcess(process).then(null, function () {
          delSession(session)
        })
      })
    }
  }

  Component.onCompleted: {
    App.setActions(actions)
    App.setMain(this)
    App.setSettings(settings.settings)

    showMaximized()
    actions.updateSessionActions(false)

    pm.initRecords()

    doOpenFileOrPrj('/home/chenyong/my/loginsight/core/test/assets/sample.log')
  }

  function doOpenFileOrPrj(url, name) {
    if (!name)
      name = url.substring(url.lastIndexOf('/'))
    const session = addSession(name)
    //TODO: check if it is log or prj
    session.coreReady.connect(function () {
      session.openFile(url).then(null, function () {
        delSession(session)
      })
    })
  }

  function openFileOrPrj() {
    openDlg.visible = true
  }

  function openProcess() {
    pmDlg.visible = true
  }

  function openClipboard() {
    const path = NativeHelper.writeClipboardToTemp();
    doOpenFileOrPrj(path)
  }

  function currentLogView() {
    return currentSession().currentLogView()
  }

  function currentSession() {
    return sessions.itemAt(sessions.currentIndex)
  }

  function addSession(title) {
    const tabBarButton = Qt.createComponent(
                         'qrc:/ClosableTabButton.qml').createObject(tabBar, {
                                                                      "title": title
                                                                    })
    const session = Qt.createComponent(
                    'qrc:/Session.qml').createObject(sessions)

    tabBarButton.closed.connect(function () {
      delSession(session)
    })

    session.meta.tabBtn = tabBarButton
    tabBar.currentIndex = tabBar.count - 1

    return session
  }

  function delSession(session) {
    session.meta.tabBtn.destroy()
    session.destroy()
  }

  function showSettings() {
    settings.visible = true
  }
}
