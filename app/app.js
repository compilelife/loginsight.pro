.pragma library

const settings = {
  logView: {
    font: {
      size: 14,
      family: 'monospace'
    },
    lineSpacing: 5
  }
}

//js in qml can't directly set global property
//so we provide setters here
let actions = null
function setActions(o){actions = o}

let main = null
function setMain(o){main = o}

let currentSession = null
function setCurrentSession(o){currentSession = o}
function isCurrentSession(o) {return currentSession === o}

let currentLogView = null
function setCurrentView(o){currentLogView = o}
