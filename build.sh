#!/bin/bash
set -e

FILES="src/*.c ayumi/ayumi.c"
FLAGS="-std=c11 -Wall -Wextra -Wpedantic -lz -lm `sdl2-config --libs`"
FLAGS_DEBUG="-Og -ggdb"
FLAGS_RELEASE="-O3 -ffast-math -flto -fwhole-program"
WIN32_FILES="src/win32/*.c src/win32/*.o -static `sdl2-config --static-libs`"
WIN32_FLAGS="-DPLATFORM_WIN32"
RELEASE_FILES="sleepdart* rom/* palettes/*"

while getopts ':p:dr' opt; do
    case $opt in
        (p) PLATFORM=$OPTARG;;
        (d) DEBUG=1;;
        (r) RELEASEPKG=1;;
        (:) :;;
    esac
done

if [[ $PLATFORM == "WIN32" ]] || [[ $PLATFORM == "win32" ]]; then
    FILES="$FILES $WIN32_FILES"
    FLAGS="$FLAGS $WIN32_FLAGS"

    windres src/win32/resource.rc src/win32/resource.o
fi

if [[ $DEBUG == "1" ]]; then
    FLAGS="$FLAGS $FLAGS_DEBUG"
else
    FLAGS="$FLAGS $FLAGS_RELEASE"
fi

set +e
DESCRIBE=`git describe --tags --dirty --always`
set -e
if [[ $? -eq 0 ]]; then
    FLAGS="$FLAGS -DGIT_DESCRIBE=\"$DESCRIBE\""
else
    DESCRIBE=0
fi

echo "FILES: $FILES"
echo "FLAGS: $FLAGS"
gcc $FILES $FLAGS -o sleepdart

if [[ $RELEASEPKG == "1" ]]; then
    if [[ "$DESCRIBE" == "0" ]]; then
        PKGNAME="sleepdart.zip"
    else
        PKGNAME="sleepdart-$DESCRIBE.zip"
    fi

    zip "$PKGNAME" $RELEASE_FILES
fi
