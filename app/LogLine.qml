import QtQuick 2.0

//when use as Repeater's child, this could be reused
Item {
  property var model: null
  property int lineNumWidth: 0

  id: root
  width: parent.width
  height: model === null ? 0 : loader.height

  //act as vue v-if
  Loader {
    id: loader
  }

  onModelChanged: {
    if (model) {
      loader.sourceComponent = lineImpl
    } else {
      loader.sourceComponent = null
    }
  }

  Component {
    //only visible when model not null
    id: lineImpl
    Row {
      spacing: 8
      Rectangle {
        id: indicator
        width: lineNumWidth + 4
        height: content.height
        color: '#49b2f6'
        Text {
          width: parent.width
          horizontalAlignment: Text.AlignHCenter
          text: model.index + 1
          wrapMode: Text.NoWrap
        }
      }
      Text {
        id: content
        width: root.width - indicator.width
        text: model.content
        wrapMode: Text.WrapAnywhere
      }
    }
  }
}
