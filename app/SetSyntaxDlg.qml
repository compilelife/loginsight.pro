import QtQuick 2.0
import QtQuick.Controls 2.15
import './coredef.js' as CoreDef
import QtQuick.Dialogs 1.3

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

  Component.onCompleted: {
    reset('(\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}) (\\w)/([a-zA-Z. _-]+)\\(( *\\d+)\\)',
          [
            {type: CoreDef.SegTypeDate, name: 'time', color: 'red'},
            {type: CoreDef.SegTypeLogLevel, name: 'level', color: 'blue'},
            {type: CoreDef.SegTypeStr, name: 'process', color: 'green'},
            {type: CoreDef.SegTypeNum, name: 'pid', color: 'yellow'}
          ],
          [
            '12-23 14:19:59.359 I/chromium( 3941): [INFO:CONSOLE(36)] "[KMSplit:kmtvgame:28626] [OpenJS] recv data: {"cmdid":"keepAlive","sessionid":1000,"tvgameid":"dicegame"}", source: http://192.168.96.40:81/KMGame/public/split/dist/bundle.js?a4f58a91a09970ad7268 (36)',
            '12-23 14:19:59.360 I/chromium( 3941): [INFO:CONSOLE(36)] "[KMSplit:kmtvgame:28626] [OpenJS] recv resp: {"cmdid":"cb_keepAlive","sessionid":1000,"errorcode":"0","errormessage":"success","data":{}}", source: http://192.168.96.40:81/KMGame/public/split/dist/bundle.js?a4f58a91a09970ad7268 (36)',
            '',
            '12-23 14:19:59.643 I/chromium( 3941): [INFO:CONSOLE(22)] "--------trigger request keepAlive--------", source: http://192.168.96.40:81/Vod/public/Vod/common/js/toVodApi.js?ver=14 (22)',
            '12-23 14:19:59.644 I/chromium( 3941): [INFO:CONSOLE(36)] "[KMSplit:kmtvgame:28626] [OpenJS] recv data: {"cmdid":"keepAlive","sessionid":1000,"data":{},"tvgameid":"smartscreen"}", source: http://192.168.96.40:81/KMGame/public/split/dist/bundle.js?a4f58a91a09970ad7268 (36)',
            '12-23 14:19:59.644 I/chromium( 3941): [INFO:CONSOLE(36)] "[KMSplit:kmtvgame:28626] [OpenJS] recv resp: {"cmdid":"cb_keepAlive","sessionid":1000,"errorcode":"0","errormessage":"success","data":{}}", source: http://192.168.96.40:81/KMGame/public/split/dist/bundle.js?a4f58a91a09970ad7268 (36)',
            '12-23 14:19:59.646 I/chromium( 3941): [INFO:CONSOLE(45)] "[object Object]", source: http://192.168.96.40:81/Vod/public/Vod/common/js/toVodApi.js?ver=14 (45)',
          ]
    )
  }
}
