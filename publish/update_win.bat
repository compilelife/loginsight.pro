cd ..\core
rd /S /Q .xmake
rd /S /Q build
xmake f --opensource=y -m release
xmake
xmake i core