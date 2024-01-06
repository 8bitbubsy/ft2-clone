#!/bin/bash

LINUXDEPLOY="linuxdeploy-$(uname -m).AppImage"
BUILDDIR="release/linux"

rm "$BUILDDIR/$LINUXDEPLOY" &>/dev/null
rm "$BUILDDIR/ft2-clone.desktop" &>/dev/null
rm -r "$BUILDDIR/ft2-clone.AppDir" &> /dev/null
echo Compiling, please wait patiently...

mkdir -p "$BUILDDIR/ft2-clone.AppDir/usr/bin" || exit 1

gcc -DNDEBUG -DHAS_MIDI -D__LINUX_ALSA__ -DHAS_LIBFLAC src/rtmidi/*.cpp src/gfxdata/*.c src/mixer/*.c src/scopes/*.c src/modloaders/*.c src/smploaders/*.c src/libflac/*.c src/*.c -lSDL2 -lpthread -lasound -lstdc++ -lm -Wshadow -Winit-self -Wall -Wno-missing-field-initializers -Wno-unused-result -Wno-strict-aliasing -Wextra -Wunused -Wunreachable-code -Wswitch-default -Wno-stringop-overflow -O3 -o "$BUILDDIR/ft2-clone.AppDir/usr/bin/ft2-clone" || exit 1

rm src/rtmidi/*.o src/gfxdata/*.o src/*.o &> /dev/null

curl "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/$LINUXDEPLOY" -L -o "$BUILDDIR/$LINUXDEPLOY" || exit 1
chmod +x "$BUILDDIR/$LINUXDEPLOY" || exit 1

cat >"$BUILDDIR/ft2-clone.desktop" <<EOF
[Desktop Entry]
Name=ft2-clone
Exec=ft2-clone
Icon=ft2-clone
Type=Application
Categories=Audio;AudioVideo;
EOF

ROOTDIR="$PWD"
cd "$BUILDDIR"
"./$LINUXDEPLOY" --appdir ft2-clone.AppDir --output appimage --icon-file "$ROOTDIR/src/gfxdata/icon/ft2-clone.png" --icon-filename "ft2-clone" --desktop-file "ft2-clone.desktop" || exit 1

echo Done. The AppImage can be found at \'$BUILDDIR/ft2-clone-$(uname -m).AppImage\' if everything went well.
