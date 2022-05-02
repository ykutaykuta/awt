#!/bin/sh -e

DIR=$(pwd)
PROC=4
DEST_DIR="$DIR/prebuilt"
#sudo yum install autoconf automake bzip2 bzip2-devel cmake freetype-devel gcc gcc-c++ git libtool make mercurial pkgconfig zlib-devel

echo "Compiling ffmpeg on $DIR"
cd ffmpeg
export PATH="$DEST_DIR/bin:$PATH"
PKG_CONFIG_PATH="$DEST_DIR/lib/pkgconfig" \
./configure \
--cc="gcc -m64 -fPIC" \
--pkg-config-flags="--static" \
--prefix="$DEST_DIR" \
--bindir="$DEST_DIR" \
--extra-cflags="-I $DEST_DIR/include" \
--extra-ldflags="-L $DEST_DIR/lib" \
--extra-libs="-lpthread" \
--disable-everything \
--disable-x86asm \
--disable-autodetect \
--disable-shared \
--enable-pic \
--enable-static \
--enable-small \
--enable-openssl \
--enable-libxml2 \
--disable-ffprobe \
--enable-decoder=aac,mp3,pcm* \
--enable-bsf=aac_adtstoasc \
--enable-demuxer=mp3,mp4,wav,hls,dash,mpegts \
--enable-protocol=file,http,https \
--enable-parser=aac* \
--enable-filter=aresample \
--enable-demuxers
make -j$PROC
make install
hash -r
