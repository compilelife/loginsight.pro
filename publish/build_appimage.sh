cd /opt
export PATH=$PATH:/opt/qt/5.15.2/gcc_64/bin
./publish/linuxdeploy/AppRun ./publish/build/linux/appdir/usr/bin/loginsight -appimage -qmldir=./app
mv loginsight-x86_64.AppImage publish/release
rm -rf ./publish/build/linux