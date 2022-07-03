import QtQuick 2.15
import QtQuick.Window 2.15
import QtWebSockets 1.15
import QtQuick.Controls 2.15 as QC2
import QtQuick.Layouts 1.15
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Dialogs 1.3
import './app.js' as App
import './QuickPromise/promise.js' as Q

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
      MenuSeparator{}
      MenuItem {action: actions.saveProject}
      MenuSeparator{}
      MenuItem {action: actions.close}
      MenuSeparator{}
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
        currentSession().setAsCurrent()
      } else {
        App.setCurrentSession(null)
        App.setCurrentView(null)
      }
      actions.updateSessionActions(currentIndex >= 0)
    }
  }

  FileDialog {
    id: openDlg
    title: 'please choose log or project file to open'
    folder: shortcuts.home
    property var handler: null
    onAccepted: {
      const url = openDlg.fileUrl.toString().substring(7) //drop file://
      if (handler)
        handler(url)
    }
    function requestOpenFile(hint, func) {
      handler = func
      title = hint
      open()
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
      _doOpenProcess(process)
    }
  }

  Component.onCompleted: {
    App.setActions(actions)
    App.setMain(this)
    App.setSettings(settings.settings)

    showMaximized()
    actions.updateSessionActions(false)

    pm.initRecords()

//    _doOpenFileOrPrj('/home/chenyong/my/loginsight/core/test/assets/sample.log')
    _doOpenProcess('while true;do echo `date`;sleep 1;done')
  }

  function _doOpenFileOrPrj(url, name) {
    if (!name)
      name = url.substring(url.lastIndexOf('/'))
    if (name.endsWith(".liprj")) {
      return _doLoadProject(url)
    }

    const session = addSession(name)
    session.name = name

    return Q.promise(function(resolve,reject){
      session.coreReady.connect(function () {
            session.openFile(url).then(function(){
              resolve()
            }, function () {
              delSession(session)
              reject()
            })
          })
    })
  }

  function openFileOrPrj() {
    _doOpenFileOrPrj('/tmp/1.liprj')
//    openDlg.requestOpenFile('open file or project', _doOpenFileOrPrj)
  }

  function _doOpenProcess(process) {
    const session = addSession("process")
    session.name = 'process'
    //TODO: support set cache
    //TODO: support stderr
    return Q.promise(function(resolve,reject){
      session.coreReady.connect(function () {
        session.openProcess(process).then(function(){
          resolve()
        }, function () {
          delSession(session)
          reject()
        })
      })
    })
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

  function saveProject() {
    const ret = {}
    ret.sessions = []
    for (let i = 0; i < sessions.count; i++) {
      const session = sessions.itemAt(i)
      ret.sessions.push(session.onSave())
    }

    ret.currentIndex = sessions.currentIndex
    NativeHelper.writeToFile('/tmp/1.liprj', JSON.stringify(ret))
  }

  function _doLoadProject(path) {
    const prjContent = NativeHelper.readFile(path)
    if (prjContent.length === 0) {
      toast.show('project file is invalid')
      return Q.rejected()
    }

    const root = JSON.parse(prjContent)

    return _doLoadSession(root, 0).then(function(){
      tabBar.currentIndex = root.currentIndex
    },function(reason){
      toast.show(reason)
    })
  }

  function _doLoadSession(root, index) {
    if (index >= root.sessions.length)
      return Q.resolved()

    const sessionCfg = root.sessions[index]
    //FIXME: main should not know session's implementation
    const {action, arg} = sessionCfg.openArg

    let ret = null
    if (action === 'open')
      ret = _doOpenFileOrPrj(arg)
//    else if (action === 'openProcess') //restore open process has no meanings
//      ret = _doOpenProcess(arg)
    //TODO: support restore clipboard
    else
      toast.show('unknown session', action, arg)

    if (ret) {
      return ret.then(function(){
        currentSession().onLoad(sessionCfg)
        return _doLoadSession(root, index+1)
      })
    }

    return Q.rejected('some session not load')
  }
}
