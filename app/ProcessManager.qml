import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 1.4
import './app.js' as App

ColumnLayout {
  id: root
  width: parent.width

  TextField {
    id: processText
    Layout.preferredWidth: root.width
//    width: 1000//parent.width
    placeholderText: 'process needs absolute path; multi arguments can be split by space'
  }
  RowLayout {
    spacing: 10
    Text {
      text: 'cache limit'
    }
    SpinBox {
      id: cacheSpin
      minimumValue: 2
      maximumValue: 10000
      value: 100
    }
    Text {
      text: 'block (one block = 200k)'
    }
  }
  RowLayout{
    width: parent.width
    Layout.fillHeight: true
    Layout.minimumHeight: 300
    ListView {
      id: recordListView
      Layout.fillWidth: true
      Layout.preferredHeight: parent.height
      clip: true
      headerPositioning: ListView.OverlayHeader
      header: RowLayout {
        z: 10
        width: recordListView.width
        PaperText {
            Layout.fillWidth: true
            bgColor: 'lightgrey'
            text: 'process'
        }
        PaperText {
          Layout.preferredWidth: 150
          bgColor: 'lightgrey'
          text: 'cache size in blocks'
        }
      }
      delegate: Item {
        width: recordListView.width
        height: childrenRect.height
        RowLayout {
          width: parent.width
          property string bgColor: recordListView.currentIndex === index ? 'steelblue' : 'white'
          property string fgColor: recordListView.currentIndex === index ? 'white' : 'black'
          PaperText{
              bgColor: parent.bgColor
              fgColor: parent.fgColor
            Layout.fillWidth: true
            text: process
          }
          PaperText {
              bgColor: parent.bgColor
              fgColor: parent.fgColor
            Layout.preferredWidth: 150
            text: cache
          }
        }
        MouseArea {
          anchors.fill: parent
          onClicked: recordListView.currentIndex = index
        }
      }
      model: ListModel{}
      onCurrentItemChanged: {
        const v = model.get(currentIndex)
        if (v) {
          processText.text = v.process
          cacheSpin.value = v.cache
        }
      }
    }

    Column {
      Layout.alignment: Qt.AlignTop
      Button {
        width: height
        text: '+'
        onClicked: {
          appendRecord()
        }
      }
      Button {
        width: height
        text: '-'
        onClicked: {
          recordListView.model.remove(recordListView.currentIndex, 1)
          saveRecords()
        }
      }
    }
  }

  function initRecords() {
    recordListView.model.clear()
    for (const record of App.settings.processManager.records) {
      recordListView.model.append(record)
    }
  }
  function saveRecords() {
    const ret = []
    const model = recordListView.model
    for (let i = 0; i < model.count; i++) {
      ret.push(model.get(i))
    }
    App.settings.processManager.records = ret
    NativeHelper.writeToFile(NativeHelper.settingsPath(), JSON.stringify(App.settings))
  }

  function appendRecord() {
    recordListView.model.append({'process': processText.text, 'cache': cacheSpin.value})
    saveRecords()
    recordListView.currentIndex = recordListView.model.count - 1
  }

  function getUserSelect() {
    return {process: processText.text, cache: cacheSpin.value}
  }
}
