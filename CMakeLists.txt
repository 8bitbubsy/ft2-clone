cmake_minimum_required(VERSION 3.7)

project(ft2-clone)

if(EMSCRIPTEN)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web")
else()
	find_package(SDL2 REQUIRED)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ft2-clone_SOURCE_DIR}/release/other/")
endif()

if((CMAKE_CXX_COMPILER_ID MATCHES "Clang") OR (CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
	add_compile_options(-O3)
	add_compile_options(-ffast-math)
endif()

file(GLOB ft2-clone_SRC
	"${ft2-clone_SOURCE_DIR}/src/*.c"
	"${ft2-clone_SOURCE_DIR}/src/gfxdata/*.c"
	"${ft2-clone_SOURCE_DIR}/src/mixer/*.c"
	"${ft2-clone_SOURCE_DIR}/src/scopes/*.c"
	"${ft2-clone_SOURCE_DIR}/src/modloaders/*.c"
	"${ft2-clone_SOURCE_DIR}/src/smploaders/*.c"
)

if(NOT EMSCRIPTEN)
	file(GLOB ft2-clone_MIDI_SRC "${ft2-clone_SOURCE_DIR}/src/rtmidi/*.cpp")
	list(APPEND ft2-clone_SRC ${ft2-clone_MIDI_SRC})
endif()

add_executable(ft2-clone ${ft2-clone_SRC})

if(EMSCRIPTEN)
	target_compile_definitions(ft2-clone PRIVATE FT2_WEB)
	target_compile_options(ft2-clone PRIVATE -sUSE_SDL=2)
	set_target_properties(ft2-clone PROPERTIES
		OUTPUT_NAME "index.html"
		SUFFIX ""
		LINK_DEPENDS "${ft2-clone_SOURCE_DIR}/web/shell.html"
		LINK_FLAGS "-sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sENVIRONMENT=web -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=FS,ccall --shell-file=${ft2-clone_SOURCE_DIR}/web/shell.html"
	)
else()
	target_include_directories(ft2-clone SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})

	if("${SDL2_LIBRARIES}" STREQUAL "")
		message(WARNING "SDL2_LIBRARIES wasn't set, manually setting to SDL2::SDL2")
		set(SDL2_LIBRARIES "SDL2::SDL2")
	endif()

	find_package(Threads REQUIRED)
	target_link_libraries(ft2-clone PRIVATE m Threads::Threads ${SDL2_LIBRARIES})
	target_compile_definitions(ft2-clone PRIVATE HAS_MIDI)

	if(UNIX)
		if(APPLE)
			find_library(COREAUDIO CoreAudio REQUIRED)
			find_library(COREFOUNDATION CoreFoundation REQUIRED)
			find_library(COREMIDI CoreMIDI REQUIRED)
			find_library(ICONV iconv REQUIRED)
			target_link_libraries(ft2-clone PRIVATE ${COREAUDIO} ${COREMIDI} ${COREFOUNDATION} ${ICONV})
			target_compile_definitions(ft2-clone PRIVATE __MACOSX_CORE__)
		else()
			# musl systems need musl-fts installed, others have it with glibc
			find_library(FTS fts)
			if(FTS)
				target_link_libraries(ft2-clone PRIVATE fts)
			endif()
			target_link_libraries(ft2-clone PRIVATE asound)
			target_compile_definitions(ft2-clone PRIVATE __LINUX_ALSA__)
		endif()
	endif()
endif()

install(TARGETS ft2-clone RUNTIME DESTINATION bin)
