cmake_minimum_required(VERSION 3.7)

project(ft2-clone)

find_package(SDL2 REQUIRED)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ft2-clone_SOURCE_DIR}/release/other/")


file(GLOB ft2-clone_SRC
    "${ft2-clone_SOURCE_DIR}/src/rtmidi/*.cpp"
    "${ft2-clone_SOURCE_DIR}/src/*.c"
    "${ft2-clone_SOURCE_DIR}/src/gfxdata/*.c"   
)


add_executable(ft2-clone ${ft2-clone_SRC})

target_include_directories(ft2-clone SYSTEM
    PRIVATE ${SDL2_INCLUDE_DIRS})

target_link_libraries(ft2-clone 
    PRIVATE m asound pthread ${SDL2_LIBRARIES})
target_compile_definitions(ft2-clone PRIVATE __LINUX_ALSA__)


install(TARGETS ft2-clone
        RUNTIME DESTINATION bin )