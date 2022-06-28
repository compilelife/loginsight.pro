import QtQuick 2.0

Rectangle {
  id: root

  property alias bgColor: root.color
  property alias fgColor: content.color
  property alias text: content.text

  height: content.height + 20

  Text {
    id: content
    anchors {
      verticalCenter: parent.verticalCenter
      left: parent.left
      leftMargin: 10
    }
  }
}
