#!/bin/bash

rm release/other/ft2-clone &> /dev/null
echo Compiling, please wait patiently...

gcc -DNDEBUG -DHAS_MIDI -D__LINUX_ALSA__ -DHAS_LIBFLAC src/rtmidi/*.cpp src/gfxdata/*.c src/mixer/*.c src/scopes/*.c src/modloaders/*.c src/smploaders/*.c src/libflac/*.c src/*.c -lSDL2 -lpthread -lasound -lstdc++ -lm -Wshadow -Winit-self -Wall -Wno-missing-field-initializers -Wno-unused-result -Wno-strict-aliasing -Wextra -Wunused -Wunreachable-code -Wswitch-default -Wno-stringop-overflow -march=native -mtune=native -O3 -o release/other/ft2-clone

rm src/rtmidi/*.o src/gfxdata/*.o src/mixer/*.o src/scopes/*.o src/modloaders/*.o src/smploaders/*.o src/libflac/*.o src/*.o &> /dev/null

echo Done. The executable can be found in \'release/other\' if everything went well.
