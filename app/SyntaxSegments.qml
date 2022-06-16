import QtQuick 2.0
import './coredef.js' as CoreDef
import './gencolor.js' as GenColor
import QtQuick.Controls 2.15
import QtQuick.Controls 1.4 as QC1
import QtQml.Models 2.15
import QtQuick.Dialogs 1.3
import './QuickPromise/promise.js' as Q
import QtQuick.Layouts 1.15

ListView {
  spacing: 10
  height: childrenRect.height
  width: 250
  model: ListModel{}
  delegate: RowLayout {
    spacing: 5
    TextField {
      Layout.alignment: Qt.AlignVCenter
      text: name
      Layout.preferredWidth: 60
    }
    Rectangle {
      width: height
      height: typeText.height
      color: model.color
      MouseArea {
        anchors.fill: parent
        onClicked: {
          colorDlg.requestColor()
            .then(function(color){
              model.color = String(color)
            })
        }
      }
    }
    ComboBox {
      id: typeText
      Layout.fillWidth: true
      model: CoreDef.SegTypeNames
      currentIndex: type
    }
  }

  ColorDialog {
    id: colorDlg
    property var curAction: null
    function requestColor() {
      curAction = Q.promise(function(resolve, reject){
        visible = true
      })

      return curAction
    }
    onAccepted: {
      if (curAction)
        curAction.resolve(color)
    }
    onRejected: {
      if (curAction)
        curAction.reject()
    }
  }

  function reset(count) {
    model.clear()
    for (let i = 0; i < count; i++) {
      model.append({color: String(GenColor.next()), type: CoreDef.SegTypeStr, name: `field ${i+1}`})
    }
  }

  function load(segs) {
    model.clear()
    for (const seg of segs) {
      model.append(seg)
    }
  }
}
