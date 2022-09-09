cd ..\core
xmake f -m release
xmake
cd ..\publish

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
mkdir .\build\win
cd .\build\win
qmake ..\..\..\app\app.pro -spec win32-msvc "CONFIG+=qtquickcompiler"
nmake
mkdir ..\..\release\windows
windeployqt app.exe --dir ..\..\release\windows --qmldir ..\..\..\app
copy app.exe ..\..\release\windows\loginsight.exe
copy ..\..\..\core\build\windows\x64\release\core.windows.exe ..\..\release\windows
copy core.windows.exe ..\..\release\windows\
cd ..\..\
rd /S /Q build\win