.pragma library
.import './util.js' as Util

let settings = null

function setSettings(v) {
  settings = v
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

function showToast(txt, duration=2000) {
  main.toast.show(txt, duration)
}
