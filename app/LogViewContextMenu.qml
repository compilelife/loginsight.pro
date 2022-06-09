import QtQuick 2.0
import QtQuick.Controls 1.4

Menu {
  property var session: null
  property var logview: null
  property var lineModel: null
  property string selectText: ''

  readonly property bool hasSeletion: selectText.length > 0

  MenuItem {
    text: 'add to timeline'
    onTriggered: session.addToTimeLine(lineModel)
  }
  MenuItem {
    text: 'track this line'
    onTriggered: session.emphasisLine(lineModel.line)
  }

  MenuItem {
    text: 'clear highlight'
    onTriggered: session.highlightBar.clear()
  }
  MenuSeparator{}
  MenuItem {
    visible: hasSeletion
    text: 'copy'
  }
  MenuItem {
    visible: hasSeletion
    text: 'highlight'
    onTriggered: session.highlightBar.add(selectText)
  }
  MenuItem {
    visible: hasSeletion
    text: 'filter'
    onTriggered: session.filter({pattern: selectText})
  }
  MenuItem {
    visible: hasSeletion
    text: 'revert filter'
  }
}
