cd ../core
rm -rf .xmake build
xmake f --opensource=y -m release
xmake
xmake i core