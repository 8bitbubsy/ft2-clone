#!/bin/bash

# Thanks to odaki on GitHub for this script. I have modified it a bit.

arch=$(arch)
if [ $arch == "ppc" ]; then
    echo Sorry, PowerPC \(PPC\) is not supported...
fi

if [ "$1" = "-v" ]; then
    VERBOSE=-v
fi

#
# Setup variables
#
VERSION=v`grep PROG_VER_STR src/ft2_header.h|cut -d'"' -f 2`

RELEASE_MACOS_DIR=release/macos/
APP_DIR=${RELEASE_MACOS_DIR}ft2-clone-macos.app/

TARGET_X86_64=${APP_DIR}Contents/MacOS/ft2-clone-macos-x86_64
TARGET_ARM64=${APP_DIR}Contents/MacOS/ft2-clone-macos-arm64
TARGET_UNIVERSAL=${APP_DIR}Contents/MacOS/ft2-clone-macos
TARGET_DIR=${APP_DIR}Contents/MacOS/

#
# Prepare
#
if [ ! -d $TARGET_DIR ]; then
    mkdir -p $TARGET_DIR
fi

#
# Compile
#
function compile() {
    rm $1 &> /dev/null
    clang $VERBOSE $CFLAGS -F /Library/Frameworks -g0 -DNDEBUG -DHAS_MIDI -D__MACOSX_CORE__ -stdlib=libc++ src/rtmidi/*.cpp src/gfxdata/*.c src/mixer/*.c src/modloaders/*.c src/smploaders/*.c src/*.c -Winit-self -Wno-deprecated -Wextra -Wunused -mno-ms-bitfields -Wno-missing-field-initializers -Wswitch-default $LDFLAGS -L /Library/Frameworks -framework SDL2 -framework CoreMidi -framework CoreAudio -framework Cocoa -liconv -lpthread -lm -lstdc++ -o $1
    return $?
}

echo Compiling x86_64 binary, please wait patiently...
CFLAGS="-target x86_64-apple-macos10.7 -mmacosx-version-min=10.7 -arch x86_64 -mmmx -mfpmath=sse -msse2 -O3"
LDFLAGS=
export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
compile $TARGET_X86_64
if [ $? -ne 0 ]; then
    echo failed
    exit 1
fi

echo Compiling arm64 binary, please wait patiently...
CFLAGS="-target arm64-apple-macos11 -mmacosx-version-min=11.0 -arch arm64 -march=armv8.3-a+sha3 -O3"
LDFLAGS=
export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
compile $TARGET_ARM64
if [ $? -ne 0 ]; then
    echo failed
    exit 1
fi

#
# Merge binaries
#
# Reference: Building a Universal macOS Binary
#   https://developer.apple.com/documentation/xcode/building_a_universal_macos_binary
echo Building universal binary...
rm $TARGET_UNIVERAL &> /dev/null
lipo -create -output $TARGET_UNIVERSAL $TARGET_X86_64 $TARGET_ARM64
rm $TARGET_X86_64
rm $TARGET_ARM64
strip $TARGET_UNIVERSAL
install_name_tool -change @rpath/SDL2.framework/Versions/A/SDL2 @executable_path/../Frameworks/SDL2.framework/Versions/A/SDL2 $TARGET_UNIVERSAL
echo Done. The executable can be found in \'${RELEASE_MACOS_DIR}\' if everything went well.

#
# Cleanup
#
rm src/rtmidi/*.o src/gfxdata/*.o src/*.o &> /dev/null
