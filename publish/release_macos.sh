cd ../app
./create_iconset.sh
cd -

cd ../core
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
macdeployqt loginsight.app -qmldir=../../../app
cp -a ~/Qt/5.15.2/clang_64/qml/QtQuick/Controls/Styles/Desktop loginsight.app/Contents/Resources/qml/QtQuick/Controls/Styles
rm -rf release/loginsight.app
mv loginsight.app ../../release/loginsight.app
cd ../../
rm -rf build/macx
