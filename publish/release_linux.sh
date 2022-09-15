mkdir -p ./build/linux
cd ./build/linux
qmake ../../../app/app.pro -spec linux-g++ CONFIG+=qtquickcompiler
make -j
mkdir -p appdir/usr/bin
cp app appdir/usr/bin/loginsight
cp ../../../app/images/logo.png appdir/
cp ../../loginsight.desktop appdir/

cd -
cd ../core
rm -rf .xmake
xmake f -m release
xmake
cd -
cp ../core/build/linux/x86_64/release/core.linux  build/linux/appdir/usr/bin/
docker run --rm -v `pwd`:/opt/publish -v `pwd`/../app:/opt/app -v $HOME/work/devkit/qt5.15.2/:/opt/qt linuxdeploy:latest \
    /bin/bash -c /opt/publish/build_appimage.sh