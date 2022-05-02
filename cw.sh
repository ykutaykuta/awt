#!/bin/sh -e
./cf.sh
rm -rf awt
cd src
rm -rf *.so *.o
export PATH="$DEST_DIR/bin:$PATH"
DIR=$(pwd)
gcc -c -Wall awt.c -fPIC -I$DIR/../prebuilt/include/
gcc -shared *.o -o libawt.so -L$DIR/../prebuilt/lib -lavfilter -lavformat -lavcodec -lswresample -lavutil -lawt5 -lm -lpthread -lssl -lxml2 -lswscale
gcc -L$DIR -Wall main.c -o awt -lawt -lpthread
mv awt ../
cp libawt.so awt.h ../prebuilt/
echo "Completed"

