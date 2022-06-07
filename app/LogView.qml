import QtQuick 2.15
import QtQuick.Controls 2.15
import "./coredef.js" as CoreDef
import "./QuickPromise/promise.js" as Q

Item {
  id: root

  property bool checked: false
  property var exclusiveGroup: null

  property int curIndex: -1
  property var core: null
  property int logId: 0
  property var logModel: ({
                            "count": 0,
                            "range": {
                              "begin": 0,
                              "end": 0
                            },
                            "dataAt": function () {
                              return null
                            }
                          })

  property var session: null

  property int curFocusIndex: -1

  Shortcut {
    sequence: 'Ctrl+F'
    enabled: checked
    onActivated: searchBar.visible = true
  }

  onExclusiveGroupChanged: {
    if (exclusiveGroup)
      exclusiveGroup.bindCheckable(root)
  }

  onCheckedChanged: {
    console.log('check', logId, checked)
  }

  LogViewContextMenu {
    id: menu
    session: root.session
    logview: parent
  }

  Text {
    id: contentMeasure
    visible: false
    width: parent.width - 8 - indicatorMeasure.width
    wrapMode: Text.WrapAnywhere
  }

  TextMetrics {
    id: indicatorMeasure
    text: '' + logModel.range.end
  }



  MouseArea {
    id: list
    anchors.fill: parent
    enabled: false
    Column {
      id: contentHolder
      height: list.height
      clip: true
      Repeater {
        id: content
        //max line count correspond to list.height, may or may not
        model: Math.round(list.height / indicatorMeasure.height)
        LogLine {
          width: list.width
          model: logModel.dataAt(curIndex + index)
          lineNumWidth: indicatorMeasure.width
          session: root.session
          isViewChecked: root.checked
          isFocusLine: curFocusIndex === (curIndex + index)
          onContextMenu: {
            menu.selectText = select
            menu.lineModel = model
            menu.popup()
          }
          onFocusLine: {
            root.checked = true
            curFocusIndex = lineIndex
          }
        }
      }
    }

    Timer {
      id: followScorllBarTimer
      running: false
      repeat: true
      interval: 200
      onTriggered: {
        show(_positionToLineIndex(vbar.position))
      }
    }

    ScrollBar {
      id: vbar
      visible: false
      hoverEnabled: true
      active: hovered || pressed
      orientation: Qt.Vertical
      position: _lineIndexToPosition(curIndex)
      size: 0.05
      stepSize: 1 / logModel.count
      policy: ScrollBar.AsNeeded
      anchors.top: parent.top
      anchors.right: parent.right
      anchors.bottom: parent.bottom
      onPressedChanged: {
        if (vbar.pressed) {
          root.checked = true
          //user begin scrolling vbar
          followScorllBarTimer.running = true
        } else {
          //user end scrolling vbar
          followScorllBarTimer.running = false
          show(_positionToLineIndex(position))
        }
      }
    }

    function _findShownEndIndex() {
      const maxAvail = content.model

      let endIndex = curIndex
      let height = 0
      for (var i = 0; i < maxAvail; i++) {
        height += content.itemAt(i).height

        if (height > list.height)
          break
        endIndex++
      }

      return endIndex
    }

    onWheel: function (ev) {
      root.checked = true
      const indexDelta = -ev.angleDelta.y / 120
      const {beginLine, endLine} = logModel.range

        let index = curIndex + indexDelta
        if (index > endLine)
          index = endLine
        else if (index < beginLine)
          index = beginLine

        show(index)
      }
    }

  SearchBar {
    id: searchBar
    anchors.top: parent.top
    anchors.right: parent.right
    onSearch: session.search(keyword, isCaseSense)
  }

    //since vbar scroll range is 0 - 1.0
    //and log index range is range.begin - range.end
    //seems map [0,1.0] <=> [range.begin, range.end] is quite right
    //but, in fact, vbar can only drag from 0 - (1.0 - size)
    //(which means size represents how many lines already be shown)
    //A convenient approach is just map [0, 1-vbar.size] to [range.begin, range.end]
    //These are what exactly _positionToLineIndex and _lineIndexToPosition do
    function _positionToLineIndex(position) {
      return Math.floor(
            position / (1 - vbar.size) * (logModel.count - 1)) + logModel.range.begin
    }

    function _lineIndexToPosition(index) {
      return (index - logModel.range.begin) / (logModel.count - 1) * (1 - vbar.size)
    }



  function initLogModel(id, range) {
    console.log('init log model', id, range.begin, range.end)
    logId = id
    logModel = {
      "logId": logId,
      "range": range,
      "count": range.end - range.begin + 1,
      "cache": [],
      "inCache": function (begin, end = begin) {
        const cache = logModel.cache
        if (cache.length === 0)
          return false

        return cache[0].index <= begin && end <= cache[cache.length - 1].index
      },
      "dataAt": function (i) {
        const cache = logModel.cache
        return logModel.inCache(i, i) ? cache[i - cache[0].index] : null
      }
    }

    show(range.begin)
    vbar.visible = true
    list.enabled = true
  }

  function updateRange(r) {
    logModel.range = r
    logModel.count = r.end - r.begin + 1
    indicatorMeasure.text = r.end

    show(r.end, 'bottom')
  }

  function _limitRange(r) {
    if (r.begin < logModel.range.begin)
      r.begin = logModel.range.begin
    if (r.end > logModel.range.end)
      r.end = logModel.range.end
    return r
  }

  function _getShowRange(index, placeAt) {
    if (placeAt === 'bottom') {
      const prefer = _limitRange({
        begin: index - (content.model - 1),
        end: index
      })

      if (logModel.inCache(prefer.begin, prefer.end)) {
        let height = 0
        for (let i = prefer.end; i >= prefer.begin; i--) {
          contentMeasure.text = logModel.dataAt(i).content
          height += contentMeasure.height
          if (height > contentHolder.height) {
            prefer.begin = Math.min(i + 1, prefer.end)
            break
          } else if (height === contentHolder.height) {
            prefer.begin = i
            break
          }
        }
        //if not return in for loop, it means all lines' height are not enough to fill contentHolder
        //no additon op should take, just show
        return prefer
      } else {
        //if not in range,let show() function do loading, then call me again
        //next call will run into the upper branch
        return prefer
      }
    } else if (placeAt === 'middle') {
      const prefer = {
        "begin": index - 3,
        "end": index - 3 + content.model - 1
      }
      return _limitRange(prefer)
    } else {
      const prefer = {
        "begin": index,
        "end": index + content.model - 1
      }
      return _limitRange(prefer)
    }
  }

  function _getCacheRange(index) {
    const prefer = {
      "begin": index - 50,
      "end": index + 50
    }
    return _limitRange(prefer)
  }

  //if curIndex not changed, repeater won't react, that's why we need force
  //However if we call _forceRefresh once, then we should call it every time logModel changes
  function _forceRefresh(index) {
    for (let i = 0; i < content.model; i++) {
      const item = content.itemAt(i)
      item.model = logModel.dataAt(i + index)
    }
  }

  function _show(index) {
    curIndex = index
    _forceRefresh(index)
  }

  //async
  function show(index, placeAt) {
     if (logModel.range.end < logModel.range.begin)
       return Q.resolved()

    const {begin, end} = _getShowRange(index, placeAt)
    if (logModel.inCache(begin, end)) {
      _show(begin)
      return Q.resolved()
    }

    const cacheRange = _getCacheRange(index)
    return core.sendMessage(CoreDef.CmdGetLines, {
                       "logId": logModel.logId,
                       "range": cacheRange
                     })
      .then(function(msg){
         logModel.cache = msg.lines

         if (placeAt === 'bottom') {
           _show(_getShowRange(index, placeAt).begin)
         } else {
           _show(begin)
         }
      })
    }

  function _isInView(index) {
    if (!logModel.inCache(index))
        return false

    const nth = index - curIndex
    if (nth < 0 || nth >= content.model)
        return false

    //calculate real height
    let height = 0
    const maxHeight = list.height
    for (let i = 0; i <= nth; i++) {
      height += content.itemAt(i).height
    }

    return height <= maxHeight
  }

  function showIntoView(index) {
    if (_isInView(index)) {
      curFocusIndex = index
      return Q.resolved()
    } else {
      return show(index, 'middle')
        .then({curFocusIndex = index})
    }
  }

  function getSearchPos() {
    const indexInView = curFocusIndex - curIndex
    const line = content.itemAt(indexInView)
    return line ? line.getSearchPos() : {fromLine: curFocusIndex, fromChar: 0}
  }

  function showSearchResult({line, offset, len}) {
    showIntoView(line)
      .then(function(){
        for (const cacheLine of logModel.cache) {
          if (cacheLine.line === line) {
            cacheLine.searchResult = {offset, len}
          } else {
            cacheLine.searchResult = null
          }
          _forceRefresh(curIndex)
        }
      })
  }
}
