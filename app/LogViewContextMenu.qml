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
  }
  MenuItem {
    text: 'clear highlight'
  }
  MenuSeparator{}
  MenuItem {
    visible: hasSeletion
    text: 'copy'
  }
  MenuItem {
    visible: hasSeletion
    text: 'highlight'
  }
  MenuItem {
    visible: hasSeletion
    text: 'filter'
  }
  MenuItem {
    visible: hasSeletion
    text: 'revert filter'
  }
}
