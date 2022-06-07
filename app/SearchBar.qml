import QtQuick 2.0
import QtQuick.Controls 1.2
 import QtGraphicalEffects 1.15

Item {
  id: root
  width: content.width +20
  height:content.height+8

  property alias isRegex: regexBox.checked
  property alias isCaseSense: caseBox.checked

  signal search(string keyword, bool reverse, bool isContinue)

  property string _lastKeyword: null
  property bool _lastReverse: false


  onVisibleChanged: {
    _lastKeyword = null
    _lastReverse = false
  }

  Rectangle {
    id:background
    anchors.fill: parent
    color: 'white'
  }

  DropShadow {
    source:background
    anchors.fill: background
    verticalOffset: 2
    horizontalOffset: 2
    radius: 3
    color: '#80000000'
  }

  ListModel {
    id: candidateList
  }

  Row {
    id: content
    spacing: 10
    width: childrenRect.width
    height: childrenRect.height
    anchors.centerIn: parent
    ComboBox {
      id: keywordBox
      focus: root.visible
      editable: true
      currentIndex: -1
      width: 100
      model: candidateList
      textRole: 'keyword'
      onAccepted: requestSearch(editText)
      onCurrentIndexChanged: {
        const model = candidateList.get(currentIndex)
        editText = model.keyword
        caseBox.checked = model.isCaseSense
        regexBox.checked = model.isRegex
      }
    }
    IconButton {
      size: parent.height
      source: "qrc:/images/left.png"
      onClicked: requestSearch(keywordBox.editText, true)
    }
    IconButton {
      size: parent.height
      source: "qrc:/images/right.png"
      onClicked: requestSearch(keywordBox.editText)
    }
    CheckBox {
      id: caseBox
      text: 'case sense'
    }
    CheckBox {
      id: regexBox
      text: 'is regex'
    }
    IconButton {
      id: closeBtn
      size: parent.height
      source: "qrc:/images/close.png"
      onClicked: root.visible = false
    }
  }

  function _updateIsContinue(keyword, reverse) {
    const isContinue = (_lastKeyword === keyword) && (_lastReverse === reverse)
    _lastKeyword = keyword
    _lastReverse = reverse
    return isContinue
  }

  function requestSearch(keyword, reverse=false) {
    search(keyword, reverse, _updateIsContinue(keyword, reverse))
    for (let i = 0; i < candidateList.count; i++) {
      if (candidateList.get(i).keyword === keyword)
        return
    }
    candidateList.append({keyword, isRegex, isCaseSense})
  }


}
