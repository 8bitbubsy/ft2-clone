cmake_minimum_required(VERSION 3.7)

project(ft2-clone)

if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    set(SYS_EM 1)
    add_compile_options(
        -pthread
        --use-port=sdl2
    )
    add_link_options(
        -pthread
        # -sUSE_WEBGL2
        -sASYNCIFY
        -sUSE_SDL=2
        -sUSE_PTHREADS=1
        -sPTHREAD_POOL_SIZE=4
        -sALLOW_MEMORY_GROWTH
        -sINCLUDE_FULL_LIBRARY
        # -sMALLOC=mimalloc
    )
    SET(CMAKE_EXECUTABLE_SUFFIX .html)
endif()

include(CMakeDependentOption)
option(EXTERNAL_LIBFLAC "use external(system) flac library" OFF)
cmake_dependent_option(ENABLE_RTMIDI "enable MIDI support" ON SYS_EM OFF)

find_package(SDL2 REQUIRED)
string(TOLOWER ${CMAKE_BUILD_TYPE} ft2-clone_buildtype)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ft2-clone_SOURCE_DIR}/${ft2-clone_buildtype}/other/")

file(GLOB ft2-clone_SRC
    "${ft2-clone_SOURCE_DIR}/src/*.c"
    "${ft2-clone_SOURCE_DIR}/src/gfxdata/*.c"
    "${ft2-clone_SOURCE_DIR}/src/mixer/*.c"
    "${ft2-clone_SOURCE_DIR}/src/scopes/*.c"
    "${ft2-clone_SOURCE_DIR}/src/modloaders/*.c"
    "${ft2-clone_SOURCE_DIR}/src/smploaders/*.c"
)

add_executable(ft2-clone ${ft2-clone_SRC})

target_include_directories(ft2-clone SYSTEM
    PRIVATE ${SDL2_INCLUDE_DIRS})

if("${SDL2_LIBRARIES}" STREQUAL "")
    message(WARNING "SDL2_LIBRARIES wasn't set, manually setting to SDL2::SDL2")
    set(SDL2_LIBRARIES "SDL2::SDL2")
endif()

find_package(Threads REQUIRED)

target_link_libraries(ft2-clone
    PRIVATE m Threads::Threads ${SDL2_LIBRARIES})

target_compile_definitions(ft2-clone
    PRIVATE HAS_LIBFLAC)

if(UNIX)
    if(APPLE)
        find_library(COREAUDIO CoreAudio REQUIRED)
        find_library(COREFOUNDATION CoreFoundation REQUIRED)
        find_library(COREMIDI CoreMIDI REQUIRED)
        find_library(ICONV iconv REQUIRED)
        target_link_libraries(ft2-clone
            PRIVATE ${COREAUDIO} ${COREMIDI} ${COREFOUNDATION} ${ICONV})
        target_compile_definitions(ft2-clone
            PRIVATE __MACOSX_CORE__)
    else()
        if(ENABLE_RTMIDI)
            target_link_libraries(ft2-clone
                PRIVATE asound)
            target_compile_definitions(ft2-clone
                PRIVATE __LINUX_ALSA__)
        endif()
    endif()
endif()

if(ENABLE_RTMIDI)
    file(GLOB ft2-rtmidi_SRCS
        "${ft2-clone_SOURCE_DIR}/src/rtmidi/*.cpp")
    target_sources(ft2-clone PRIVATE ${ft2-rtmidi_SRCS})
    target_compile_definitions(ft2-clone PRIVATE HAS_MIDI)
endif()

if(EXTERNAL_LIBFLAC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac)
    target_compile_definitions(ft2-clone
        PRIVATE EXTERNAL_LIBFLAC)
    target_link_libraries(ft2-clone
        PRIVATE PkgConfig::FLAC)
else()
    file(GLOB flac_SRCS
        "${ft2-clone_SOURCE_DIR}/src/libflac/*.c")
    target_sources(ft2-clone PRIVATE ${flac_SRCS})
endif()

install(TARGETS ft2-clone
    RUNTIME DESTINATION bin)
