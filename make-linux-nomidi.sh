#!/bin/bash

rm release/other/ft2-clone &> /dev/null
echo Compiling \(with no MIDI functionality\), please wait patiently...

gcc -DNDEBUG src/gfxdata/*.c src/mixer/*.c src/modloaders/*.c src/smploaders/*.c src/*.c -lSDL2 -lm -Wshadow -Winit-self -Wall -Wno-missing-field-initializers -Wno-unused-result -Wno-strict-aliasing -Wextra -Wunused -Wunreachable-code -Wswitch-default -march=native -mtune=native -O3 -o release/other/ft2-clone

rm src/gfxdata/*.o src/*.o &> /dev/null

echo Done. The executable can be found in \'release/other\' if everything went well.
