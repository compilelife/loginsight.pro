import QtQuick 2.0

//when use as Repeater's child, this could be reused
Item {
  property var model: null
  property int lineNumWidth: 0
  property var segColors: []

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

  function createFormatText() {
    if (model.segs.length === 0)
        return model.content

    let ret = ''
    const ori = model.content
    let pos = 0

    for (let i = 0; i < model.segs.length; i++) {
      const {offset,length} = model.segs[i]
      if (offset > pos) {
        ret += ori.substring(pos, offset)
      }
      pos = offset +length
      ret += `<font color="${segColors[i]}">${ori.substring(offset, pos)}</font>`
    }

    if (pos < ori.length) {
      ret += ori.substring(pos)
    }

    return ret
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
        text: createFormatText()
        wrapMode: Text.WrapAnywhere
      }
    }
  }
}
