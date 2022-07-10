
import QtQuick 2.0
import QtQuick.Layouts 1.15
import QtQuick.Controls 1.4
import QtQuick.Controls 2.15 as QC2
import QtQml.Models 2.15
import './app.js' as App
import './coredef.js' as CoreDef

ColumnLayout {
  id: root
  spacing: 10
  property alias pattern: patternBox.text
  property var segs: getSegConfig()
  property alias lines: previewLines

  RowLayout {
    TextField {
      id: patternBox
      Layout.fillWidth: true
    }
    Button {
      text: 'preview'
      onClicked: previewSyntax()
    }
    Button {
      text: 'manage'
    }
  }

  RowLayout {
    width: root.width
    height: 400
    Layout.rightMargin: 5
    SyntaxSegments{
      id: syntaxSegs
      Layout.minimumWidth: 200
      Layout.alignment: Qt.AlignTop
    }
    ListView {
      id: viewRoot
      model: previewLines
      Layout.fillWidth: true
      height: parent.height
      clip: true
      Layout.alignment: Qt.AlignTop
      delegate: LogLine {
        width: viewRoot.width
        model: previewLines.get(index)
        lineNumWidth: 30
        isViewChecked: true
        session: ({syntaxSegConfig: getSegConfig(), highlights:[]})
        isFocusLine: viewRoot.currentIndex === index
        onFocusLine: viewRoot.currentIndex = lineIndex
      }
    }
  }

  ListModel {
    id: previewLines
    dynamicRoles: true
    function init(lines) {
      clear()
      let index = 0
      for (const line of lines) {
        append({index: index++, content: line.content, segs: null})
      }
    }
  }

  function previewSyntax() {
    const lines = []
    for (let i = 0; i < previewLines.count; i++) {
      lines.push(previewLines.get(i).content)
    }

    App.currentSession.core.sendMessage(CoreDef.CmdTestSyntax, {pattern, lines})
      .then(function(reply){
        for (let i = 0; i < previewLines.count; i++) {
          previewLines.setProperty(i, 'segs', reply.segs[i])
        }
        //FIXME: 上面的setProperty后，界面没刷新，下面两行起到“刷新”的作用
        viewRoot.model = null
        viewRoot.model = previewLines
      })
  }

  function getSegConfig() {
    const model = syntaxSegs.model
    const ret = []
    for (let i = 0; i < model.count; i++) {
      ret.push(model.get(i))
    }
    return ret
  }

  function setSegConfig(cfg) {
    syntaxSegs.load(cfg)
  }
}
