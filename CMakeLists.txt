cmake_minimum_required(VERSION 3.7)

project(ft2-clone)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ft2-clone_SOURCE_DIR}/release/other/")

if((CMAKE_CXX_COMPILER_ID MATCHES "Clang") OR (CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
	add_compile_options(-O3)
	add_compile_options(-ffast-math)
	add_compile_options(-fno-strict-aliasing)
endif()

set(FT2_IRIX FALSE)
if(CMAKE_SYSTEM_NAME MATCHES "^IRIX")
	set(FT2_IRIX TRUE)
endif()

option(FT2_STATIC_SDL2 "Link SDL2 statically (libSDL2.a) instead of the shared library" OFF)

find_package(SDL2 REQUIRED)

file(GLOB ft2-clone_SRC
	"${ft2-clone_SOURCE_DIR}/src/*.c"
	"${ft2-clone_SOURCE_DIR}/src/gfxdata/*.c"
	"${ft2-clone_SOURCE_DIR}/src/mixer/*.c"
	"${ft2-clone_SOURCE_DIR}/src/scopes/*.c"
	"${ft2-clone_SOURCE_DIR}/src/modloaders/*.c"
	"${ft2-clone_SOURCE_DIR}/src/smploaders/*.c"
)

if(NOT FT2_IRIX)
	file(GLOB ft2-clone_MIDI_SRC "${ft2-clone_SOURCE_DIR}/src/rtmidi/*.cpp")
	list(APPEND ft2-clone_SRC ${ft2-clone_MIDI_SRC})
endif()

add_executable(ft2-clone ${ft2-clone_SRC})

find_package(Threads REQUIRED)

target_include_directories(ft2-clone SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})

if(FT2_STATIC_SDL2)
	# sdl2-config lists the libraries a static SDL2 build depends on (X11, GL, audio, ...)
	find_program(SDL2_CONFIG_EXECUTABLE sdl2-config)
	if(NOT SDL2_CONFIG_EXECUTABLE)
		message(FATAL_ERROR "FT2_STATIC_SDL2 needs sdl2-config to find SDL2's static dependencies")
	endif()

	execute_process(COMMAND ${SDL2_CONFIG_EXECUTABLE} --prefix OUTPUT_VARIABLE SDL2_CONFIG_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND ${SDL2_CONFIG_EXECUTABLE} --static-libs OUTPUT_VARIABLE SDL2_STATIC_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
	separate_arguments(SDL2_STATIC_FLAGS UNIX_COMMAND "${SDL2_STATIC_FLAGS}")

	# replace -lSDL2 with the full path to libSDL2.a, so the linker can't pick the shared library instead
	set(SDL2_STATIC_LIBDIRS "")
	set(SDL2_STATIC_DEPS "")
	foreach(flag ${SDL2_STATIC_FLAGS})
		if(flag MATCHES "^-L(.+)")
			list(APPEND SDL2_STATIC_LIBDIRS "${CMAKE_MATCH_1}")
			list(APPEND SDL2_STATIC_DEPS "${flag}")
		elseif(NOT (flag STREQUAL "-lSDL2" OR flag STREQUAL "-lSDL2main"))
			list(APPEND SDL2_STATIC_DEPS "${flag}")
		endif()
	endforeach()

	find_library(SDL2_STATIC_LIBRARY NAMES libSDL2.a
		HINTS ${SDL2_STATIC_LIBDIRS} "${SDL2_CONFIG_PREFIX}/lib32" "${SDL2_CONFIG_PREFIX}/lib")
	if(NOT SDL2_STATIC_LIBRARY)
		message(FATAL_ERROR "FT2_STATIC_SDL2 is ON, but libSDL2.a wasn't found")
	endif()

	message(STATUS "Linking SDL2 statically: ${SDL2_STATIC_LIBRARY}")
	target_link_libraries(ft2-clone PRIVATE ${SDL2_STATIC_LIBRARY} ${SDL2_STATIC_DEPS} m Threads::Threads)
else()
	if("${SDL2_LIBRARIES}" STREQUAL "")
		message(WARNING "SDL2_LIBRARIES wasn't set, manually setting to SDL2::SDL2")
		set(SDL2_LIBRARIES "SDL2::SDL2")
	endif()

	target_link_libraries(ft2-clone PRIVATE m Threads::Threads ${SDL2_LIBRARIES})
endif()

if(NOT FT2_IRIX)
	target_compile_definitions(ft2-clone PRIVATE HAS_MIDI)
endif()

if(UNIX)
	if(APPLE)
		find_library(COREAUDIO CoreAudio REQUIRED)
		find_library(COREFOUNDATION CoreFoundation REQUIRED)
		find_library(COREMIDI CoreMIDI REQUIRED)
		find_library(ICONV iconv REQUIRED)
		target_link_libraries(ft2-clone PRIVATE ${COREAUDIO} ${COREMIDI} ${COREFOUNDATION} ${ICONV})
		target_compile_definitions(ft2-clone PRIVATE __MACOSX_CORE__)
	elseif(FT2_IRIX)
		# No ALSA, no MIDI backend on IRIX.
		# Add iconv for IRIX
		find_library(ICONV iconv REQUIRED)
		target_link_libraries(ft2-clone PRIVATE ${ICONV})
	else()
		target_link_libraries(ft2-clone PRIVATE asound)
		target_compile_definitions(ft2-clone PRIVATE __LINUX_ALSA__)
	endif()
endif()

install(TARGETS ft2-clone RUNTIME DESTINATION bin)
