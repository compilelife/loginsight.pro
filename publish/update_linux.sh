cd ../core
rm -rf .xmake
xmake f --opensource=y -m release
xmake
xmake i core