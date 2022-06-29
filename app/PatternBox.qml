import QtQuick 2.0
import QtQuick.Controls 1.4

Row {
  property alias isRegex: regexBox.checked
  property alias isCaseSense: caseSenseBox.checked
  property alias curText: keywordBox.editText
  signal accept

  spacing: 10

  onVisibleChanged: {
    if(visible)
      keywordBox.forceActiveFocus()
  }

  ComboBox {
    id: keywordBox
    focus: true
    editable: true
    currentIndex: -1
    model: ListModel {}
    textRole: 'keyword'
    onActivated: {
      const data = model.get(currentIndex)
      if (data) {
        const {keyword, regex, caseSense} = data
        editText = keyword
        regexBox.checked = regex
        caseSense.checked = caseSense
      }
    }
    onAccepted: accept()
  }

    CheckBox {
      id: caseSenseBox
      text: 'case sense'
    }

    CheckBox {
      id: regexBox
      text: 'is regex'
    }

    function addCurToCompleter() {
      if (curText.length === 0)
          return

      const { model } = keywordBox
      for (var i = 0; i < model.count; i++) {
        if (model.get(i).keyword === curText)
          return
      }

      model.append({
                     "keyword": curText,
                     "caseSense": isCaseSense,
                     "regex": isRegex
                   })
    }
}
