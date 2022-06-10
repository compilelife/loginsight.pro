//{index, placeAt}
let history = []
//the next call to goBack return this index
let currentIndex = -1

function canGoBack() {
  return currentIndex >= 0 && history.length > 0
}

function canGoForward() {
  return currentIndex < history.length - 1 && history.length > 0
}

function isInOldTime() {
  return canGoForward()
}

function addPos(pos) {
  if (isInOldTime()) {
    //drop newer pos from currentIndex
    history = history.slice(0, currentIndex + 1)
  }
  history.push(pos)
  currentIndex = history.length - 1
}

function goBack() {
  return history[currentIndex--]
}

function goForward() {
  return history[++currentIndex]
}
