import QtQuick 2.0
import QtQuick.Controls 2.15

Dialog {
  property var range: ({begin: 0, end: 0})
  property int index: valueBox.value - 1

  standardButtons: Dialog.Ok | Dialog.Cancel
  title: 'goto'

  contentItem: Column {
    spacing: 10
    Text {
      text: `please input a number from ${range.begin+1} to ${range.end+1}`
    }
    SpinBox {
      id: valueBox
      property int realValue: value - 1
      editable: true
      from: range.begin + 1
      to: range.end + 1
    }
  }

  function setIndex(index) {
    valueBox.value = index + 1
  }
}
