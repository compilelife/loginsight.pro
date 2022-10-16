cd ../app
./create_iconset.sh
cd -

cd ../core
rm -rf .xmake
xmake f -m release
xmake
cd -

mkdir ./build/macx
cd ./build/macx
export PATH=~/Qt/5.15.2/clang_64/bin/:$PATH
qmake ../../../app/app.pro -spec macx-clang CONFIG+=x86_64 CONFIG+=qtquickcompiler
make -j
mv app.app loginsight.app
cp ../../../core/build/macosx/x86_64/release/core.macosx loginsight.app/Contents/MacOS/
mv loginsight.app/Contents/MacOS/app loginsight.app/Contents/MacOS/loginsight 
macdeployqt loginsight.app -no-strip -qmldir=../../../app
rm -rf ../../release/loginsight.app
mv loginsight.app ../../release/loginsight.app
cd ../../
cd release
zip -r macosx.zip loginsight.app # 不能在pack里压缩，因为macos下的软连接在linux下并不是被识别为软连接，压缩后还原给macos就会变成普通文件，出现magic word
cd -
rm -rf build/macx
