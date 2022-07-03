import QtQuick 2.0
import QtQuick.Controls 1.4
import './app.js' as App

Item {
  property var sessionActions: [
    close,
    filter,
    search,
    goTo,
    clearTimeLine,
    shotTimeLine,
    setSyntax,
    followLog,
    saveProject
  ]

  function updateSessionActions(hasSession) {
    const enable = hasSession
    for (const action of sessionActions) {
      action.enabled = enable
    }
  }

  property Action open:   Action {
    text: 'open file or project'
    shortcut: 'ctrl+o'
    onTriggered: App.main.openFileOrPrj()
  }

  property Action openProcess: Action {
    text: 'open process'
    onTriggered: App.main.openProcess()
  }

  property Action close:   Action {
    text: 'close tab'
    shortcut: 'ctrl+w'
    onTriggered: App.main.delSession(App.currentSession)
  }

  property Action filter:   Action {
    text: 'filter'
    shortcut: 'ctrl+d'
    iconSource: 'qrc:/images/filter.png'
    onTriggered: App.currentLogView.filterAction()
  }

  property Action search:   Action {
    text: 'search'
    shortcut: 'ctrl+f'
    iconSource: 'qrc:/images/search.png'
    onTriggered: App.currentLogView.searchAction()
  }

  property Action goTo:  Action {
    text: 'goto'
    iconSource: 'qrc:/images/locate.png'
    shortcut: 'ctrl + g'
    onTriggered: App.currentLogView.gotoAction()
  }

  property Action clearTimeLine:  Action {
    text: 'clear timeline'
    iconSource: 'qrc:/images/clear.png'
    onTriggered: App.currentSession.timeline.clear()
  }

  property Action shotTimeLine: Action {
    text: 'shot timeline'
    iconSource: 'qrc:/images/screenshot.png'
    onTriggered: App.currentSession.timeline.screenShot()
  }

  property Action goBack: Action {
    text: 'go back'
    enabled: false
    iconSource: 'qrc:/images/left.png'
    shortcut: 'ctrl+['
    onTriggered: App.currentLogView.goBack()
  }

  property Action goForward: Action {
    text: 'go forward'
    enabled: false
    iconSource: 'qrc:/images/right.png'
    shortcut: 'ctrl+]'
    onTriggered: App.currentLogView.goForward()
  }

  property Action setSyntax: Action {
    text: 'set syntax'
     onTriggered: App.currentSession.showSyntaxDlg()
  }

  property Action settings: Action {
    text: 'settings'
    onTriggered: App.main.showSettings()
  }

  property Action followLog: Action {
    text: 'followLog'
    checkable: true
    checked: true
    shortcut: 'ctrl+m'
    iconSource: 'qrc:/images/pause.png'
    onCheckedChanged: {
      if (checked)
        iconSource = 'qrc:/images/pause.png'
      else
        iconSource = 'qrc:/images/play.png'
      App.currentSession.setFollowLog(checked)
    }
  }

  property Action openClipboard: Action {
    text: 'open clipboard'
    onTriggered: App.main.openClipboard()
  }

  property Action saveProject: Action {
    text: 'save project'
    onTriggered: App.main.saveProject()
  }
}
