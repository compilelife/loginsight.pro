import QtQuick 2.0
import QtQuick.Controls 1.2
 import QtGraphicalEffects 1.15

Item {
  id: root
  width: content.width +20
  height:content.height+8

  property alias isRegex: regexBox.checked
  property alias isCaseSense: caseBox.checked

  signal search(string keyword, bool reverse)

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
      model: []
      onAccepted: search(editText, false)
      onActivated: search(model[index], false)
    }
    IconButton {
      size: parent.height
      source: "qrc:/images/left.png"
      onClicked: search(keywordBox.editText, true)
    }
    IconButton {
      size: parent.height
      source: "qrc:/images/right.png"
      onClicked: search(keywordBox.editText, false)
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
}
