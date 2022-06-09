import QtQuick 2.0
import com.cy.LineHighlighter 1.0
import QtQuick 2.15
import './app.js' as App

//when use as Repeater's child, this could be reused
Item {
  property var model: null
  property int lineNumWidth: 0
  property var session: null
  property bool isViewChecked: false
  property bool isFocusLine: false

  id: root
  width: parent.width
  height: model === null ? 0 : loader.height

  signal contextMenu(var model, string select)
  signal focusLine(int lineIndex)
  signal emphasisLine(int line)

  property TextEdit _content: null

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
        height: content.height + App.settings.logView.lineSpacing
        color: isViewChecked ? '#49b2f6' : 'grey'
        Text {
          width: parent.width
          horizontalAlignment: Text.AlignLeft
          text: isFocusLine ? String(model.index + 1)+' >>' : String(model.index + 1)
          wrapMode: Text.NoWrap
          color: isFocusLine ? 'white' : 'black'
        }
      }
      TextEdit {
        id: content
        focus:true
        textFormat: TextEdit.RichText
//        readOnly: true
        selectByMouse: true
        width: root.width - indicator.width
        text: model.content
        wrapMode: Text.WrapAnywhere
        font {
          family: App.settings.logView.font.family
          pixelSize: App.settings.logView.font.size
        }

        MouseArea{
          anchors.fill: parent
          acceptedButtons: Qt.RightButton
          onClicked: {
            if (mouse.button === Qt.RightButton) {
              content.forceActiveFocus()
              contextMenu(model, content.selectedText)
            }
          }
        }
        LineHighlighter {
          id: highlighter
          segColors: session.segColors
          highlights: session.highlights
          segs: model.segs
          searchResult: model.searchResult || {}
        }
        Component.onCompleted: {
          highlighter.setup(content.textDocument)
          _content = content
        }
        onActiveFocusChanged: {
          if (activeFocus)
            focusLine(model.index)
        }
      }
    }
  }

  function hasActiveFocus() {
    return _content.activeFocus
  }

  function getSearchPos() {
    return {fromLine: model.index, fromChar: _content.cursorPosition}
  }
}
