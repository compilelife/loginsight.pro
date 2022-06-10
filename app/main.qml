import QtQuick 2.15
import QtQuick.Window 2.15
import QtWebSockets 1.15
import QtQuick.Controls 2.15
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

  property bool hasSession: tabBar.currentIndex >= 0

  menuBar: MenuBar {
    id: menubar
    Menu {
      title: "File"
      MenuItem {
        text: 'open'
        onTriggered: openDlg.visible = true
      }
      MenuItem {
        text: "close"
        enabled: hasSession
        onTriggered: delSession(currentSession())
      }
    }
    Menu {
      title: "Insight"
      enabled: hasSession
      MenuItem {
        text: 'filter'
        onTriggered: currentSession().filterAction()
      }
      MenuItem {
        text: 'search'
        onTriggered: currentSession().searchAction()
      }
      MenuItem {
        text: 'goto'
        onTriggered: currentSession().gotoAction()
      }
    }
    Menu {
      title: "TimeLine"
      enabled: hasSession
      MenuItem {
        text: 'clear'
        onTriggered: currentSession().timeline.clear()
      }
      MenuItem {
        text: 'screenshot timeline'
        onTriggered: currentSession().timeLine.screenShot()
      }
    }
  }

  TabBar {
    contentHeight: 26
    id: tabBar
  }
  StackLayout {
    id: sessions
    currentIndex: tabBar.currentIndex
    anchors.top: tabBar.bottom
    width: parent.width
    height: parent.height - tabBar.height
  }

  FileDialog {
    id: openDlg
    title: 'please choose log or project file to open'
    folder: shortcuts.home
    onAccepted: {
      const url = openDlg.fileUrl.toString().substring(7) //drop file://
      const name = url.substring(url.lastIndexOf('/'))
      const session = addSession(name)
      //TODO: check if log or prj
      session.coreReady.connect(function () {
        session.openFile(url).then(null, function () {
          delSession(session)
        })
      })
    }
  }

  Component.onCompleted: {
    showMaximized()

    const url = '/home/chenyong/my/loginsight/core/test/assets/sample.log'
    const name = url.substring(url.lastIndexOf('/'))
    const session = addSession(name)
    //TODO: check if log or prj
    session.coreReady.connect(function () {
      session.openFile(url).then(null, function () {
        delSession(session)
      })
    })
  }

//  Session{}

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
}
