import QtQuick 2.0
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.5
import QtQml.Models 2.12
import QtQuick.Controls.Styles 1.4

Item {
    id: root
    signal doubleClickNode(int line)

    ColorIndicator {
        id: currentColor
        color: 'blue'
    }

    ListModel {
        id: nodes
    }

    Axis {
        anchors.left: timeline.left
        anchors.leftMargin: 80
        anchors.top: timeline.top
        anchors.bottom: timeline.bottom
    }

    ListView {
        id: timeline
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: currentColor.bottom
        anchors.topMargin: 20
        anchors.bottom: parent.bottom
        spacing: 20
        model: nodes
        delegate: Item{//制作一个边距
            width: timeline.width
            height: node.height+20
            TimeNode{
                id: node
                line: model.line
                log: model.log
                comment: model.comment
                color: model.color

                width: parent.width-20
                anchors.margins: 10
                anchors.centerIn: parent
                onRecommendHeight: v=>{
                    height = v
                }
                onClicked: {
                    timeline.currentIndex = index
                }
                onDoubleClicked: {
                    doubleClickNode(nodes.get(index).line)
                }
                onRequestDelete: {
                    nodes.remove(index)
                }
            }
        }
        highlight: Rectangle {
            border.color: '#40FF0000'
            border.width: 3
            color: 'transparent'
            z:10
        }
        highlightResizeDuration: 300
        highlightMoveDuration: 300
    }

    function saveToJson() {
        var allNodes = []
        for (var i = 0; i < nodes.count; i++) {
            allNodes.push(nodes.get(i))
        }

        return JSON.stringify(allNodes)
    }

    function loadFromJson(json) {
        var allNodes = JSON.parse(json)
        allNodes.forEach(node=>nodes.append(node))
    }

    function addNode(line, log) {
        var node = {line, log, comment:'备注： ', color: currentColor.color}
        let index = nodes.count
        for (let i = 0; i < nodes.count; i++) {
            const nodeLine = nodes.get(i).line
            if (nodeLine === line) {
                index = i
                break;
            } else if (nodeLine > line) {
                nodes.insert(i, node)
                index = i;
                break;
            }
        }

        if (index === nodes.count) {
            nodes.append(node)
        }

        timeline.currentIndex = index
    }

    function clear() {
        nodes.clear()
    }

    property int currentIndex: -1
    function beginScreenShot() {
        currentColor.visible = false
        currentIndex = timeline.currentIndex
        timeline.currentIndex = -1
    }

    function endScreenShot() {
        currentColor.visible = true
        timeline.currentIndex = currentIndex
    }

//    Row {//调试按钮
//        visible: true
//        anchors.bottom: parent.bottom
//        Button {
//            text: 'to json'
//            onClicked: console.log(root.exportToJson())
//        }
//        Button {
//            text: '添加'
//            onClicked: addNode(1, 'hello')
//        }
//    }
}
