import QtQuick 2.0
import QtQuick.Controls 2.15

TabButton {
    property string title: ''
    signal closed()

    contentItem: Row {
        Text {
            text: title
            verticalAlignment: Text.AlignVCenter
            height: parent.height
        }
        Image {
            height: parent.height
            fillMode: Image.PreserveAspectFit
            source: "qrc:/images/close.png"
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    closed()
                }
            }
        }
    }
}
