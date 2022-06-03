import QtQuick 2.0
import com.cy.LineHighlighter 1.0
import QtQuick 2.15

//when use as Repeater's child, this could be reused
Item {
  property var model: null
  property int lineNumWidth: 0
  property var session: null

  id: root
  width: parent.width
  height: model === null ? 0 : loader.height

  signal contextMenu(var model, string select)

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
          color: content.activeFocus ? 'white' : 'black'
        }
      }
      TextEdit {
        id: content
        textFormat: TextEdit.RichText
        readOnly: true
        selectByMouse: true
        width: root.width - indicator.width
        text: model.content
        wrapMode: Text.WrapAnywhere
        MouseArea{
          anchors.fill: parent
          acceptedButtons: Qt.RightButton
          onClicked: {
            content.forceActiveFocus()
            contextMenu(model, content.selectedText)
          }
        }
        LineHighlighter {
          id: highlighter
          segColors: session.segColors
          highlights: session.highlights
          segs: model.segs
        }
        Component.onCompleted: {
          highlighter.setup(content.textDocument)
        }
      }
    }
  }
}
