#!/bin/bash

arch=$(arch)
if [ $arch == "ppc" ]; then
    echo Sorry, PowerPC \(PPC\) is not supported...
else
    echo Compiling 64-bit binary, please wait patiently...
    
    rm release/macos/ft2-clone-macos.app/Contents/MacOS/ft2-clone-macos &> /dev/null
    
    clang -mmacosx-version-min=10.7 -arch x86_64 -mmmx -mfpmath=sse -msse2 -I/Library/Frameworks/SDL2.framework/Headers -F/Library/Frameworks -g0 -DNDEBUG -D__MACOSX_CORE__ -stdlib=libc++ src/rtmidi/*.cpp src/gfxdata/*.c src/*.c -O3 /usr/lib/libiconv.dylib -lm -Winit-self -Wno-deprecated -Wextra -Wunused -mno-ms-bitfields -Wno-missing-field-initializers -Wswitch-default -framework SDL2 -framework CoreMidi -framework CoreAudio -framework Cocoa -lpthread -lm -lstdc++ -o release/macos/ft2-clone-macos.app/Contents/MacOS/ft2-clone-macos
    strip release/macos/ft2-clone-macos.app/Contents/MacOS/ft2-clone-macos
    install_name_tool -change @rpath/SDL2.framework/Versions/A/SDL2 @executable_path/../Frameworks/SDL2.framework/Versions/A/SDL2 release/macos/ft2-clone-macos.app/Contents/MacOS/ft2-clone-macos
    
    rm src/rtmidi/*.o src/gfxdata/*.o src/*.o &> /dev/null
    echo Done! The executable is in the folder named \'release/macos\'.
fi
