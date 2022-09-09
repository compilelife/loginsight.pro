cd release
zip -r macosx.zip loginsight.app

mv windows loginsight
zip -r windows.zip loginsight

zip -r linux.zip *.AppImage

rm -rf loginsight*