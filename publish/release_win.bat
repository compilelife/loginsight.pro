mkdir ..\build
cd ..\build
qmake ..\loginsight.pro -spec win32-msvc "CONFIG+=qtquickcompiler"
nmake
mkdir artifact
dir src
windeployqt src\release\loginsight.exe --dir artifact --qmldir ..\src\qml
copy src\release\loginsight.exe artifact\
