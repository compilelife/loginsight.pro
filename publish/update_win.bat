cd ..\core
rd /S /Q .xmake
xmake f --opensource=y -m release
xmake
xmake i core