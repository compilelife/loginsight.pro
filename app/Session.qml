import QtQuick 2.0
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import './coredef.js' as CoreDef
import QtQml.Models 2.15
import QtQml 2.15
import QtQuick.Dialogs 1.3
import './util.js' as Util
import './app.js' as App

Item {
  id: root
  signal coreReady()

  property Core core: Core {
        onReady: {
            core.serverCmdHandlers[CoreDef.ServerCmdRangeChanged] = handleLogRangeChanged
          coreReady()
        }
    }

    property var meta: ({})

    property var logMap: ({})

    property var timeline: timelineImpl
    property var highlightBar: highlightBarImpl

    property var highlights: []
    property alias syntaxSegConfig: setSyntax.segs

    property var logExclusive : null

  property bool followLog: true

    Component.onCompleted: {
      logExclusive = Qt.createQmlObject('import QtQuick.Controls 1.4; ExclusiveGroup{}', root, 'logExclusive')
    }

    HighlightBar {
      id: highlightBarImpl
      width: parent.width
      onChanged: {
        highlights = getHighlights()
        invalidate()
      }
      onFilter: {
        root.filter({pattern: keyword})
      }
      onSearch: {
        root.search({pattern: keyword})
      }
    }

    SplitView {
      anchors.top: highlightBarImpl.bottom
      height: parent.height - highlightBarImpl.height
        width: parent.width
        orientation: Qt.Horizontal
        SplitView {
            height: parent.height
            orientation: Qt.Vertical
            SplitView.fillWidth: true

            LogView {
                id: rootLogView
                core:root.core
                width: parent.width
                SplitView.fillHeight: true
                SplitView.minimumHeight: 200
                SplitView.preferredHeight: 400
            }
            ColumnLayout {
                id: subLogs
                width: parent.width
                SplitView.preferredHeight: 300
                visible: tabBar.count > 0
                TabBar {
                    id: tabBar
                    contentHeight: 26
                }
                StackLayout {
                    id: holder
                    Layout.fillHeight: true
                    currentIndex: tabBar.currentIndex
                }

                function append(id, range) {
                    const tabBarButton = Qt.createComponent('qrc:/ClosableTabButton.qml')
                                            .createObject(tabBar, {title: 'tab'})
                    const subLog = Qt.createComponent('qrc:/LogView.qml')
                                      .createObject(holder, {core})

                    subLog.initLogModel(id, range)
                    _onLogAdded(id, subLog)

                    tabBarButton.closed.connect(function(){
                        tabBarButton.destroy()
                        _onLogRemoved(subLog.logId)
                        subLog.destroy()
                    })

                  tabBar.currentIndex = tabBar.count - 1
                }
            }
        }
        TimeLine {
          id: timelineImpl
            height: parent.height
            SplitView.preferredWidth: 400
            visible: !empty

           onDoubleClickNode: {
             emphasisLine(line)
           }
        }
    }

  MessageDialog {
    id: errTip
    standardButtons: MessageDialog.Ok
    function display(title, detail) {
      errTip.title = title
      errTip.text = detail
      errTip.visible = true
    }
  }

  Dialog {
    id: setSyntaxDlg
    title: 'setting syntax'
    standardButtons: StandardButton.Ok | StandardButton.Cancel
    width: 800
    height: 500
    SetSyntax {
      id: setSyntax
      width: parent.width
    }

    function show() {
      setSyntax.lines.init(rootLogView.getTopLines(30))
      visible = true
    }

    onAccepted: {
      core.sendMessage(CoreDef.CmdSetLineSegment, {
                                                pattern: setSyntax.pattern,
                                                segs: setSyntax.segs,
                                                caseSense: true
                                              })
            .then(function() {
              onSyntaxChanged()
            })
       }
   }

  function showSyntaxDlg() {
    setSyntaxDlg.show()
  }

    function _onLogAdded(logId, logView) {
        logMap[logId]=logView
      logView.session = root
      logView.exclusiveGroup = logExclusive
      logView.checked = true
    }
    function _getLogView(logId) {
        return logMap[logId]
    }
    function _onLogRemoved(logId) {
        delete logMap[logId]
    }

    function openFile(path) {
        return core.sendModalMessage(CoreDef.CmdOpenFile, {path})
            .then(msg=>{
                rootLogView.initLogModel(msg.logId, msg.range)
                _onLogAdded(msg.logId, rootLogView)
            })
    }

    function openProcess(process) {
      core.sendMessage(CoreDef.CmdOpenProcess, {process})
        .then(function(msg){
          rootLogView.initLogModel(msg.logId, msg.range)
          _onLogAdded(msg.logId, rootLogView)
        })
    }

    //param: {pattern, caseSense, regex}
    function filter(param) {
      const curLog = currentLogView()
      const filterArg = Util.merge({
                                     logId: curLog.logId,
                                     caseSense: true,
                                     regex: false,
                                   }, param)
        core.sendModalMessage(CoreDef.CmdFilter, filterArg)
            .then(msg=>{
                subLogs.append(msg.logId, msg.range)
            })
    }

    //param: {pattern, caseSense, reverse, regex}
    function search(param, searchPos = null) {
      const curLog = currentLogView()
      const {fromLine,fromChar} = searchPos ? searchPos : curLog.getSearchPos()
      const searchArg = Util.merge({
        logId: curLog.logId,
        fromLine,
        fromChar,
        pattern: '',
        caseSense: true,
        reverse: false,
        regex: false
      }, param)

      core.sendModalMessage(CoreDef.CmdSearch, searchArg)
        .then(function(msg){
          if (msg.found) {
            curLog.showSearchResult(msg)
          } else {
            //TODO: more specific log, such as 'search down to bottom not found'
            errTip.display('search error', searchArg.pattern + ' not found')
          }
        })
    }

    function handleLogRangeChanged(msg) {
      if (!followLog)
        return

        core.sendMessage(CoreDef.CmdSyncLogs)
            .then(function(msg){
                for (const r of msg.ranges) {
                    const logView = _getLogView(r.logId)
                    if (logView)
                        logView.updateRange(r.range)
                }
            })
    }

    function addToTimeLine(lineModel) {
      timeline.addNode(lineModel.line, lineModel.content)
    }

    function currentLogView() {
      for (const key in logMap) {
        if (logMap[key].checked)
           return logMap[key]
      }
      return rootLogView
    }

    function emphasisLine(line) {
      core.sendMessage(CoreDef.CmdMapLine, {logId: rootLogView.logId, index: line})
        .then(function(msg){
          for (const {logId,index} of msg.lines) {
            logMap[logId].showIntoView(index, {remember: true})
          }
          timeline.highlightNode(line)
        })
    }

    function onSyntaxChanged() {
      for (const key in logMap) {
        logMap[key].onSyntaxChanged()
      }
    }

    function invalidate() {
      for (const key in logMap) {
        logMap[key].invalidate()
      }
    }

    function setAsCurrent() {
      updateActions()
      App.setCurrentView(currentLogView())
    }

    function updateActions() {
      App.actions.followLog.checked = followLog
    }

    function setFollowLog(v) {
      const shouldMoveToBottom = v && !followLog
      followLog = v

      if (shouldMoveToBottom) {
        for (const key in logMap) {
          logMap[key].moveToBottom()
        }
      }
    }
}
