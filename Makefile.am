# ft2-clone Makefile.am

# Binary name
bin_PROGRAMS = ft2-clone

# Source files - core
ft2_clone_SOURCES = \
	src/ft2_about.c \
	src/ft2_audio.c \
	src/ft2_audioselector.c \
	src/ft2_bmp.c \
	src/ft2_checkboxes.c \
	src/ft2_config.c \
	src/ft2_diskop.c \
	src/ft2_edit.c \
	src/ft2_events.c \
	src/ft2_gui.c \
	src/ft2_help.c \
	src/ft2_hpc.c \
	src/ft2_inst_ed.c \
	src/ft2_keyboard.c \
	src/ft2_main.c \
	src/ft2_midi.c \
	src/ft2_module_loader.c \
	src/ft2_module_saver.c \
	src/ft2_mouse.c \
	src/ft2_nibbles.c \
	src/ft2_palette.c \
	src/ft2_pattern_draw.c \
	src/ft2_pattern_ed.c \
	src/ft2_pushbuttons.c \
	src/ft2_radiobuttons.c \
	src/ft2_random.c \
	src/ft2_replayer.c \
	src/ft2_sample_ed.c \
	src/ft2_sample_ed_features.c \
	src/ft2_sample_loader.c \
	src/ft2_sample_saver.c \
	src/ft2_sampling.c \
	src/ft2_scrollbars.c \
	src/ft2_smpfx.c \
	src/ft2_structs.c \
	src/ft2_sysreqs.c \
	src/ft2_tables.c \
	src/ft2_textboxes.c \
	src/ft2_trim.c \
	src/ft2_unicode.c \
	src/ft2_video.c \
	src/ft2_wav_renderer.c

# Graphics data
ft2_clone_SOURCES += \
	src/gfxdata/ft2_bmp_fonts.c \
	src/gfxdata/ft2_bmp_gui.c \
	src/gfxdata/ft2_bmp_instr.c \
	src/gfxdata/ft2_bmp_logo.c \
	src/gfxdata/ft2_bmp_looppins.c \
	src/gfxdata/ft2_bmp_midi.c \
	src/gfxdata/ft2_bmp_mouse.c \
	src/gfxdata/ft2_bmp_nibbles.c \
	src/gfxdata/ft2_bmp_scopes.c

# Mixer
ft2_clone_SOURCES += \
	src/mixer/ft2_cubic_spline.c \
	src/mixer/ft2_mix.c \
	src/mixer/ft2_quadratic_spline.c \
	src/mixer/ft2_silence_mix.c \
	src/mixer/ft2_windowed_sinc.c

# Scopes
ft2_clone_SOURCES += \
	src/scopes/ft2_scopedraw.c \
	src/scopes/ft2_scopes.c

# Module loaders
ft2_clone_SOURCES += \
	src/modloaders/ft2_load_bem.c \
	src/modloaders/ft2_load_digi.c \
	src/modloaders/ft2_load_it.c \
	src/modloaders/ft2_load_mod.c \
	src/modloaders/ft2_load_s3m.c \
	src/modloaders/ft2_load_stk.c \
	src/modloaders/ft2_load_stm.c \
	src/modloaders/ft2_load_xm.c

# Sample loaders
ft2_clone_SOURCES += \
	src/smploaders/ft2_load_aiff.c \
	src/smploaders/ft2_load_brr.c \
	src/smploaders/ft2_load_flac.c \
	src/smploaders/ft2_load_iff.c \
	src/smploaders/ft2_load_raw.c \
	src/smploaders/ft2_load_wav.c

# MIDI sources are now platform-specific (see below)

# Conditional FLAC support
if HAS_LIBFLAC
ft2_clone_SOURCES += \
	src/libflac/bitmath.c \
	src/libflac/bitreader.c \
	src/libflac/crc.c \
	src/libflac/fixed.c \
	src/libflac/format.c \
	src/libflac/lpc.c \
	src/libflac/md5.c \
	src/libflac/memory.c \
	src/libflac/metadata_iterators.c \
	src/libflac/metadata_object.c \
	src/libflac/stream_decoder.c \
	src/libflac/window.c \
	src/libflac/windows_unicode_filenames.c
endif

# Compiler flags
AM_CFLAGS = @SDL2_CFLAGS@ @ALSA_CFLAGS@ @FLAC_CFLAGS@
AM_CXXFLAGS = @SDL2_CFLAGS@ @ALSA_CFLAGS@ @FLAC_CFLAGS@

# Linker flags
ft2_clone_LDADD = @SDL2_LIBS@ @ALSA_LIBS@ @FLAC_LIBS@ @COREAUDIO_LIBS@ -lpthread -lm -lstdc++ -liconv

# Platform-specific flags
if PLATFORM_LINUX
AM_CFLAGS += -D__LINUX_ALSA__
endif

if PLATFORM_DARWIN
AM_CFLAGS += -D__MACOSX_CORE__
AM_CXXFLAGS += -stdlib=libc++
endif

# Platform-specific source files
if PLATFORM_LINUX
ft2_clone_SOURCES += src/rtmidi/rtmidi_c.cpp src/rtmidi/RtMidi.cpp
endif

if PLATFORM_DARWIN
ft2_clone_SOURCES += src/rtmidi/rtmidi_c.cpp src/rtmidi/RtMidi.cpp
endif

# Debug flags
if DEBUG
AM_CFLAGS += -g -DDEBUG
AM_CXXFLAGS += -g -DDEBUG
else
AM_CFLAGS += -DNDEBUG
AM_CXXFLAGS += -DNDEBUG
endif

# Clean targets
CLEANFILES = ft2-clone
DISTCLEANFILES = config.log config.status

# Desktop file installation (optional)
# if LINUX
# desktopdir = $(datadir)/applications
# desktop_DATA = release/other/Freedesktop.org\ Resources/ft2-clone.desktop
# 
# icondir = $(datadir)/icons/hicolor/48x48/apps
# icon_DATA = release/other/Freedesktop.org\ Resources/ft2-clone.png
# endif

# Help target for build scripts compatibility
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all          - Build ft2-clone"
	@echo "  clean        - Remove build artifacts"
	@echo "  distclean    - Remove all generated files"
	@echo "  install      - Install ft2-clone"
	@echo "  uninstall    - Remove ft2-clone"
	@echo ""
	@echo "Build options:"
	@echo "  --with-midi=auto|yes|no  - MIDI support (default: auto)"
	@echo "  --with-flac=auto|yes|no  - FLAC support (default: auto)"
	@echo "  --enable-debug           - Enable debug build"
