#!/bin/bash

rm release/other/ft2-clone &> /dev/null
echo Compiling \(with no MIDI functionality\), please wait patiently...

# If you're compiling for *SLOW* devices, try adding -DLERPMIX right after gcc
# This will activate 2-tap linear interpolation mixing (blurrier sound) instead
# of 3-tap quadratic interpolation mixing (sharper sound)

gcc -DNDEBUG src/gfxdata/*.c src/*.c -lSDL2 -lm -Wshadow -Winit-self -Wall -Wno-missing-field-initializers -Wno-unused-result -Wno-strict-aliasing -Wextra -Wunused -Wunreachable-code -Wswitch-default -march=native -mtune=native -O3 -o release/other/ft2-clone

rm src/gfxdata/*.o src/*.o &> /dev/null

echo Done! The executable is in the folder named \'release/other\'.
