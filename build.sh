#!/bin/bash
set -e

FILES="*.c ayumi/ayumi.c"
FLAGS="-std=c11 -Wall -Wextra -Wpedantic -Wno-unused-parameter --static -lz `sdl2-config --libs --static-libs`"
FLAGS_DEBUG="-ggdb"
FLAGS_RELEASE="-O2"
WIN32_FILES="win32/*.c win32/*.o"
WIN32_FLAGS="-DPLATFORM_WIN32"

while getopts ':p:d' opt; do
    case $opt in
        (p) PLATFORM=$OPTARG;;
        (d) DEBUG=1;;
        (:) :;;
    esac
done

if [[ $PLATFORM == "WIN32" ]] || [[ $PLATFORM == "win32" ]]; then
    FILES="$FILES $WIN32_FILES"
    FLAGS="$FLAGS $WIN32_FLAGS"

    windres win32/resource.rc win32/resource.o
fi

if [[ $DEBUG == "1" ]]; then
    FLAGS="$FLAGS $FLAGS_DEBUG"
else
    FLAGS="$FLAGS $FLAGS_RELEASE"
fi

set +e
DESCRIBE=`git describe --dirty --always`
set -e
if [[ $? -eq 0 ]]; then
    FLAGS="$FLAGS -DGIT_DESCRIBE=\"$DESCRIBE\""
fi

echo "FILES: $FILES"
echo "FLAGS: $FLAGS"
gcc $FILES $FLAGS -o sleepdart

