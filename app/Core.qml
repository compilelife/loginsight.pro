import QtQuick 2.0
import QtWebSockets 1.15
import QtQuick.Controls 1.4
import "./QuickPromise/promise.js" as Q
import com.cy.CoreBoot 1.0
import "./coredef.js" as CoreDef
import QtQuick.Dialogs 1.3
import './util.js' as Util

Item {
  property var pendings: ({})
  property var serverCmdHandlers: ({})
  property WebSocket channel: null
  property int idGen: 0

  signal ready

  CoreBoot {
    id: boot
    onStateChanged: function (running) {
      console.log('state changed to ', running)
      if (running) {
        createChannel()
      } else {
        coreErrDlg.showError('websocketd disconnected, please relaunch')
      }
    }
  }

  Component.onCompleted: {
    boot.startLocal()
  }

  Component.onDestruction: {
    boot.stop()
  }

  MessageDialog {
    id: coreErrDlg
    visible: false
    standardButtons: StandardButton.Ok
    title: 'core error'

    property var then: null
    function showError(content) {
      coreErrDlg.text = content
      coreErrDlg.visible = true
    }

    onVisibleChanged: {
      if (!coreErrDlg.visible && then) {
        then()
      }
    }
  }

  //shown when op take more than 200ms
  Dialog {
    id: longOpDlg
    property string waitId: ''
    property int waitPromiseId: 0
    property string hint: ''
    property bool enabled: false

    title: 'proceeding long op'
    standardButtons: StandardButton.Cancel
    Column {
      Text {
        text: longOpDlg.hint
      }
      ProgressBar {
        id: progressBar
        minimumValue: 0
        maximumValue: 100
        value: 0
      }
      Timer {
        id: checkProgressTimer
        interval: 200
        repeat: true
        running: longOpDlg.enabled
        onTriggered: {
          console.log('timer sent')
          sendMessage(CoreDef.CmdQueryPromise, {pid: longOpDlg.waitPromiseId})
            .then(function(msg){
              console.log('timer', longOpDlg.waitId)
              progressBar.value = msg.progress
              longOpDlg.visible = true
            })
        }
      }
    }

    onRejected: {
      sendMessage(CoreDef.CmdCancelPromise)
      enabled = false
    }

    function startWait(id) {
      waitId = id
      enabled = true
    }

    function finishWait() {
      visible = false
      enabled = false
    }
  }

  function sendModalMessage(cmd, extra) {
    const ret = sendMessage(cmd, extra)
    longOpDlg.startWait(`ui-${idGen}`)
    return ret
  }

  function sendMessage(cmd, extra) {
    const packed = Util.merge({
                 "id": `ui-${++idGen}`,
                 "cmd": cmd
               }, extra)

    const ret = Q.promise(function (resolve, reject) {
      pendings[packed.id] = {
        "resolve": resolve,
        "reject": reject
      }
    })

    const msg = JSON.stringify(packed)
    console.log('send', msg)
    channel.sendTextMessage(msg)

    return ret
  }

  function replyMessage(msg, extra) {
    const packed = {
      "id": msg.id,
      "cmd": CoreDef.CmdReply,
      "state": CoreDef.StateOk,
      origin: msg.cmd
    }
    if (extra) {
      for (const field in extra)
        packed[field] = extra[field]
    }

    const content = JSON.stringify(packed)
    console.log('reply', content)
    channel.sendTextMessage(content)
  }

  function _findIndexOf(arr, predict) {
    for (var i = 0; i < arr.length; i++) {
      if (predict(arr[i]))
        return i
    }
    return -1
  }

  function _onTextMsg(msg) {
    let msgObj = null
    try {
      msgObj = JSON.parse(msg)
    } catch (e) {
      console.info('stdout', msg)
      return
    }

    const cmd = msgObj.cmd
    console.log('recv', msg)
    if (cmd === CoreDef.CmdReply) {
      if (msgObj.state === CoreDef.StateOk
          || msgObj.state === CoreDef.StateCancel) {
        const pending = pendings[msgObj.id]
        if (pending) {
          pending.resolve(msgObj)
          delete pendings[msgObj.id]
        }
      } else if (msgObj.state === CoreDef.StateFail) {
        const pending = pendings[msgObj.id]
        if (pending) {
          coreErrDlg.then = ()=>{
            pending.reject(msgObj)
            delete pendings[msgObj.id]
          }
        }
        coreErrDlg.showError(msgObj.why)
      }

      if (msgObj.state === CoreDef.StateFuture) {
        if (longOpDlg.enabled && longOpDlg.waitId === msgObj.id) {
          longOpDlg.waitPromiseId = msgObj["pid"]
        }
      } else {
        if (longOpDlg.enabled && longOpDlg.waitId === msgObj.id) {
          longOpDlg.finishWait()
        }
      }
    } else {
      const handler = serverCmdHandlers[cmd]
      if (handler) {
        const handleRet = handler(msgObj)
        replyMessage(msgObj, handleRet)
      }
    }
  }

  function createChannel() {
    channel = Qt.createQmlObject(`import QtWebSockets 1.15;WebSocket{}`, this,
                                 'coreChannel')
    channel.textMessageReceived.connect(_onTextMsg)
    channel.statusChanged.connect(function (status) {
      console.log("websocket connect status:", status)
      if (status === WebSocket.Open) {
        ready()
      } else if (status === WebSocket.Error) {
        console.log(channel.errorString)
        coreErrDlg.showError('core disconnected, please relaunch')
      }
    })
    channel.url = boot.url
    channel.active = true
  }
}
