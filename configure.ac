# Process this file with autoconf to produce a configure script.
# You may need to use autoconf 2.56 or newer

# Require autoconf 2.69
AC_PREREQ([2.69])

AC_INIT(ft2-clone, 1.40)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

# silent build by default
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Default install dir
AC_PREFIX_DEFAULT(/usr/local)

# Checks for programs
AC_PROG_CC
AC_PROG_CXX

# Check for pkg-config
PKG_PROG_PKG_CONFIG

# Check for required SDL2
AC_SUBST([SDL2_CFLAGS])
AC_SUBST([SDL2_LIBS])
PKG_CHECK_MODULES([SDL2], [sdl2])

# Check for zlib (optional, for better compatibility)
AC_SUBST([ZLIB_CFLAGS])
AC_SUBST([ZLIB_LIBS])
PKG_CHECK_MODULES([ZLIB], [zlib], [has_zlib=yes], [has_zlib=no])
if test "x$has_zlib" = "xno"; then
    ZLIB_CFLAGS=""
    ZLIB_LIBS=""
fi

# Platform detection
AC_CANONICAL_HOST

# Initialize feature flags
has_midi=no
has_flac=no
has_alsa=no
has_coreaudio=no

# Platform-specific audio backend detection
case $host_os in
    linux*)
        AC_SUBST([ALSA_CFLAGS])
        AC_SUBST([ALSA_LIBS])
        PKG_CHECK_MODULES([ALSA], [alsa], [has_alsa=yes], [has_alsa=no])
        if test "x$has_alsa" = "xno"; then
            AC_MSG_WARN([ALSA not found, audio may not work properly])
        fi
        ;;
    darwin*)
        has_coreaudio=yes
        AC_SUBST([COREAUDIO_LIBS], ["-framework CoreAudio -framework CoreMidi -framework Cocoa"])
        ;;
    *)
        AC_MSG_WARN([Unsupported platform: $host_os])
        ;;
esac

# Optional MIDI support
AC_ARG_WITH([midi], 
    AS_HELP_STRING([--with-midi], [Build with MIDI support (default: auto)]),
    [with_midi=$withval], [with_midi=auto])

if test "x$with_midi" != "xno"; then
    case $host_os in
        linux*)
            if test "x$has_alsa" = "xyes"; then
                has_midi=yes
                AC_DEFINE([HAS_MIDI], [1], [Define to 1 if MIDI support is enabled])
                AC_DEFINE([__LINUX_ALSA__], [1], [Define to 1 for Linux ALSA MIDI])
            fi
            ;;
        darwin*)
            has_midi=yes
            AC_DEFINE([HAS_MIDI], [1], [Define to 1 if MIDI support is enabled])
            AC_DEFINE([__MACOSX_CORE__], [1], [Define to 1 for macOS Core MIDI])
            ;;
    esac
fi

# Optional FLAC support
AC_ARG_WITH([flac], 
    AS_HELP_STRING([--with-flac], [Build with FLAC support (default: auto)]),
    [with_flac=$withval], [with_flac=auto])

if test "x$with_flac" != "xno"; then
    # Check for system FLAC first
    PKG_CHECK_MODULES([FLAC], [flac], [has_flac=yes], [has_flac=no])
    
    if test "x$has_flac" = "xno"; then
        # Fall back to bundled libflac
        if test -d "src/libflac"; then
            has_flac=yes
            AC_DEFINE([HAS_LIBFLAC], [1], [Define to 1 if using bundled FLAC])
            AC_SUBST([FLAC_CFLAGS], [""])
            AC_SUBST([FLAC_LIBS], [""])
        fi
    else
        AC_DEFINE([HAS_FLAC], [1], [Define to 1 if using system FLAC])
    fi
fi

# Compiler flags
case $host_os in
    linux*)
        CFLAGS="$CFLAGS -march=native -mtune=native -O3"
        CFLAGS="$CFLAGS -Wshadow -Winit-self -Wall -Wno-missing-field-initializers"
        CFLAGS="$CFLAGS -Wno-unused-result -Wno-strict-aliasing -Wextra -Wunused"
        CFLAGS="$CFLAGS -Wunreachable-code -Wswitch-default -Wno-stringop-overflow"
        ;;
    darwin*)
        CFLAGS="$CFLAGS -O3"
        CFLAGS="$CFLAGS -Winit-self -Wno-deprecated -Wextra -Wunused"
        CFLAGS="$CFLAGS -mno-ms-bitfields -Wno-missing-field-initializers"
        CXXFLAGS="$CXXFLAGS -stdlib=libc++"
        ;;
esac

# Debug build option
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debug build]),
    [enable_debug=$enableval], [enable_debug=no])

if test "x$enable_debug" = "xyes"; then
    CFLAGS="$CFLAGS -g -DDEBUG"
    AC_DEFINE([DEBUG], [1], [Define to 1 for debug build])
else
    CFLAGS="$CFLAGS -DNDEBUG"
    AC_DEFINE([NDEBUG], [1], [Define to 1 for release build])
fi

# Summary
echo ""
echo "Build configuration:"
echo "  Platform: $host_os"
echo "  MIDI support: $has_midi"
echo "  FLAC support: $has_flac"
echo "  ALSA support: $has_alsa"
echo "  CoreAudio support: $has_coreaudio"
echo "  Debug build: $enable_debug"
echo ""

# Set platform variables for Makefile.am
case $host_os in
    linux*)
        PLATFORM_LINUX=yes
        PLATFORM_DARWIN=no
        ;;
    darwin*)
        PLATFORM_LINUX=no
        PLATFORM_DARWIN=yes
        ;;
    *)
        PLATFORM_LINUX=no
        PLATFORM_DARWIN=no
        ;;
esac
AC_SUBST([PLATFORM_LINUX])
AC_SUBST([PLATFORM_DARWIN])

# Set feature conditionals
AM_CONDITIONAL([HAS_MIDI], [test "$has_midi" = "yes"])
AM_CONDITIONAL([HAS_LIBFLAC], [test "$has_flac" = "yes"])
AM_CONDITIONAL([DEBUG], [test "$enable_debug" = "yes"])
AM_CONDITIONAL([PLATFORM_LINUX], [test "$PLATFORM_LINUX" = "yes"])
AM_CONDITIONAL([PLATFORM_DARWIN], [test "$PLATFORM_DARWIN" = "yes"])
AM_CONDITIONAL([LINUX], [test "$PLATFORM_LINUX" = "yes"])

# Write Makefile
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
