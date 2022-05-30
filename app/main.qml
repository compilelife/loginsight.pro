import QtQuick 2.15
import QtQuick.Window 2.15
import QtWebSockets 1.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

ApplicationWindow {
    width: 800
    height: 700
    visible: true
    title: qsTr("Loginsight")

    menuBar: MenuBar {
        id: menubar
        Menu {
            title: "File"
             MenuItem {
                 text: 'open'
                 onTriggered: {
//                     session.openFile('/home/chenyong/my/loginsight/core/test/assets/sample.log')
//                     session.openFile('/tmp/1.txt')
                   session.openProcess('while true;do echo `date`; sleep 1;done')
                 }
             }

             MenuItem {
                 text: "Close"
                 onTriggered: {
                 }
             }
        }
        Menu {
            title: "Insight"
            MenuItem {
                text: 'filter'
                onTriggered: {
                    session.filter()
                }
            }
        }
    }

    TabBar {
      contentHeight: 22
      ClosableTabButton{ title: 'abc' }
      ClosableTabButton{ title: 'efg' }
      TabButton { text: 'xyz' }
    }

//    Session {
//        id: session
//        anchors.fill: parent
//    }
}
