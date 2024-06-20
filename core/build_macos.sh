xmake clean
xmake f -c
xmake 
cp build/macosx/arm64/release/core.macosx ../app/core.darwin

xmake clean
xmake f -c -a x86_64
xmake
cp build/macosx/x86_64/release/core.macosx ../app/core.darwin.x64