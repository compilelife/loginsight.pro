import QtQuick 2.0
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import './coredef.js' as CoreDef
import QtQml.Models 2.15
import QtQml 2.15
import QtQuick.Dialogs 1.3

Item {
  id: root
  signal coreReady()

    Core {
        id: core
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
    property var segColors: ["blue","green","red","yellow"]

    property var logExclusive : null
    Component.onCompleted: {
      logExclusive = Qt.createQmlObject('import QtQuick.Controls 1.4; ExclusiveGroup{}', root, 'logExclusive')
    }

    HighlightBar {
      id: highlightBarImpl
      width: parent.width
      onChanged: {
        highlights = getHighlights()
        //TODO: force refresh all logview here
      }
      onFilter: {
        root.filter(keyword, true)
      }
      onSearch: {
        root.search(keyword, true)
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
                core:core
                width: parent.width
                SplitView.minimumHeight: 200
                SplitView.preferredHeight: 400
            }
            Column {
                id: subLogs
                width: parent.width
                SplitView.fillHeight: true
                SplitView.preferredHeight: 300
                TabBar {
                    id: tabBar
                    contentHeight: 26
                }
                StackLayout {
                    id: holder
                    currentIndex: tabBar.currentIndex
                    width: parent.width
                    height: subLogs.height - tabBar.height
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
            implicitWidth: 200
        }
    }

  MessageDialog {
    id: errTip
    standardButtons: StandardButton.OK
    function display(title, detail) {
      errTip.title = title
      errTip.text = detail
      errTip.visible = true
    }
  }

    function _onLogAdded(logId, logView) {
        logMap[logId]=logView
      logView.session = root
      logView.exclusiveGroup = logExclusive
      logView.checked = true//FIXME: not working?
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
                core.sendMessage(CoreDef.CmdSetLineSegment, {
                                   pattern:'(\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}) (\\w)/([a-zA-Z. _-]+)\\(( *\\d+)\\)',
                                   caseSense: false,
                                   segs: [
                                     {type: CoreDef.SegTypeDate, name: 'time'},
                                     {type: CoreDef.SegTypeLogLevel, name: 'level'},
                                     {type: CoreDef.SegTypeStr, name: 'process'},
                                     {type: CoreDef.SegTypeNum, name: 'pid'}
                                   ]
                                 })
            })
    }

    function openProcess(process) {
      core.sendMessage(CoreDef.CmdOpenProcess, {process})
        .then(function(msg){
          rootLogView.initLogModel(msg.logId, msg.range)
          _onLogAdded(msg.logId, rootLogView)
        })
    }

    function filter(pattern, caseSense) {
        core.sendModalMessage(CoreDef.CmdFilter,
                              {regex: false,
                                  caseSense,
                                  pattern,
                                  logId: _getCurLogView().logId
                              })
            .then(msg=>{
                subLogs.append(msg.logId, msg.range)
            })
    }

    function search(pattern, caseSense) {
      const curLog = _getCurLogView()
      const {fromLine,fromChar} = curLog.getSearchPos()
      const searchArg = {
        logId: curLog.logId,
        fromLine,
        fromChar,
        pattern,
        caseSense,
        reverse: false,
        regex: false
      }
      core.sendModalMessage(CoreDef.CmdSearch, searchArg)
        .then(function(msg){
          if (msg.found) {
            curLog.show(msg.line, 'middle')
            //TODO: replace with showIntoView(msg.line)
            //which keep line unchanged while already in view, or display line in middle of view if invisible
            //then make it currentline to highlight
          } else {
            //TODO: more specific like search down to bottom not found)
            errTip.display('search error', pattern + 'not found')
          }
        })
    }

    function handleLogRangeChanged(msg) {
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
      timeline.addNode(lineModel.line+1, lineModel.content)
    }

    function _getCurLogView() {
      for (const key in logMap) {
        if (logMap[key].checked)
           return logMap[key]
      }
      return rootLogView
    }
}
