cd release

mv windows loginsight
zip -r windows.zip loginsight

zip -r linux.zip *.AppImage

rm -rf loginsight*