import QtQuick 2.0
import QtQuick.Controls 2.15
import './coredef.js' as CoreDef
import QtQuick.Dialogs 1.3
import './app.js' as App

Dialog {
  title: 'setting syntax'
  standardButtons: StandardButton.Ok | StandardButton.Cancel
  width: 800
  height: 500
  SetSyntax {
    id: setSyntax
    width: parent.width
  }

  function reset(pattern, segs, lines) {
    setSyntax.pattern = pattern || ''
    setSyntax.segs.load(segs || [])
    setSyntax.lines.init(lines)//required.
  }

  onAccepted: {
    App.currentSession.core.sendMessage(CoreDef.CmdSetLineSegment, {
                                          pattern: setSyntax.pattern,
                                          segs: setSyntax.segs,
                                          caseSense: true
                                        })
      .then(function() {
        App.currentSession.onSyntaxChanged()//TODO: test this
      })
  }
}
