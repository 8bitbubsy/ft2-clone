#!/bin/bash

# ft2-clone Release Builder
# Creates AppImages for Linux and .app bundles for macOS using autotools

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --linux              Build Linux AppImage"
    echo "  --macos              Build macOS .app bundle"
    echo "  --all                Build for all platforms (default)"
    echo "  --no-midi            Build without MIDI support"
    echo "  --no-flac            Build without FLAC support"
    echo "  --debug              Build debug version"
    echo "  --clean              Clean build directories first"
    echo "  --help               Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 --linux           # Build Linux AppImage only"
    echo "  $0 --macos --no-midi # Build macOS app without MIDI"
    echo "  $0 --all --clean     # Build everything with full features"
}

# Default values
BUILD_LINUX=false
BUILD_MACOS=false
BUILD_ALL=true
NO_MIDI=false
NO_FLAC=false
DEBUG=false
CLEAN=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --linux)
            BUILD_LINUX=true
            BUILD_ALL=false
            shift
            ;;
        --macos)
            BUILD_MACOS=true
            BUILD_ALL=false
            shift
            ;;
        --all)
            BUILD_ALL=true
            shift
            ;;
        --no-midi)
            NO_MIDI=true
            shift
            ;;
        --no-flac)
            NO_FLAC=true
            shift
            ;;
        --debug)
            DEBUG=true
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Set build targets
if [ "$BUILD_ALL" = true ]; then
    BUILD_LINUX=true
    BUILD_MACOS=true
fi

# Detect platform
PLATFORM=$(uname -s)
ARCH=$(uname -m)

print_status "ft2-clone Release Builder"
print_status "Platform: $PLATFORM ($ARCH)"
print_status "Build targets: Linux=$BUILD_LINUX, macOS=$BUILD_MACOS"

# Check if we're on the right platform for macOS builds
if [ "$BUILD_MACOS" = true ] && [ "$PLATFORM" != "Darwin" ]; then
    print_error "macOS builds can only be created on macOS systems"
    exit 1
fi

# Clean if requested
if [ "$CLEAN" = true ]; then
    print_status "Cleaning build directories..."
    rm -rf release/linux release/macos
    make clean 2>/dev/null || true
fi

# Prepare configure arguments
CONFIGURE_ARGS=""
if [ "$NO_MIDI" = true ]; then
    CONFIGURE_ARGS="$CONFIGURE_ARGS --with-midi=no"
fi
if [ "$NO_FLAC" = true ]; then
    CONFIGURE_ARGS="$CONFIGURE_ARGS --with-flac=no"
fi
if [ "$DEBUG" = true ]; then
    CONFIGURE_ARGS="$CONFIGURE_ARGS --enable-debug"
fi

# Function to build with autotools
build_autotools() {
    local platform=$1
    local features=$2
    
    print_status "Building for $platform with features: $features"
    
    # Regenerate autotools files
    print_status "Regenerating autotools files..."
    autoreconf -fi
    
    # Configure
    print_status "Configuring build..."
    ./configure $CONFIGURE_ARGS
    
    # Build
    print_status "Compiling..."
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    print_success "Build completed for $platform"
}

# Function to create Linux AppImage
create_linux_appimage() {
    print_status "Creating Linux AppImage..."
    
    LINUXDEPLOY="linuxdeploy-$(uname -m).AppImage"
    BUILDDIR="release/linux"
    
    # Clean previous builds
    rm -f "$BUILDDIR/$LINUXDEPLOY"
    rm -f "$BUILDDIR/ft2-clone.desktop"
    rm -rf "$BUILDDIR/ft2-clone.AppDir"
    
    # Create build directory
    mkdir -p "$BUILDDIR/ft2-clone.AppDir/usr/bin"
    
    # Copy binary
    cp ft2-clone "$BUILDDIR/ft2-clone.AppDir/usr/bin/"
    
    # Download linuxdeploy if not present
    if [ ! -f "$BUILDDIR/$LINUXDEPLOY" ]; then
        print_status "Downloading linuxdeploy..."
        curl -L "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/$LINUXDEPLOY" -o "$BUILDDIR/$LINUXDEPLOY"
        chmod +x "$BUILDDIR/$LINUXDEPLOY"
    fi
    
    # Create desktop file
    cat >"$BUILDDIR/ft2-clone.desktop" <<EOF
[Desktop Entry]
Name=ft2-clone
Exec=ft2-clone
Icon=ft2-clone
Type=Application
Categories=Audio;AudioVideo;
Comment=FastTracker 2 compatible tracker
EOF
    
    # Create AppImage
    ROOTDIR="$PWD"
    cd "$BUILDDIR"
    "./$LINUXDEPLOY" \
        --appdir ft2-clone.AppDir \
        --output appimage \
        --icon-file "$ROOTDIR/src/gfxdata/icon/ft2-clone.png" \
        --icon-filename "ft2-clone" \
        --desktop-file "ft2-clone.desktop" || {
        print_error "Failed to create AppImage"
        exit 1
    }
    
    cd "$ROOTDIR"
    
    # Find the created AppImage
    APPIMAGE=$(find "$BUILDDIR" -name "ft2-clone-*.AppImage" | head -1)
    if [ -n "$APPIMAGE" ]; then
        print_success "AppImage created: $APPIMAGE"
    else
        print_error "AppImage not found"
        exit 1
    fi
}

# Function to create macOS app bundle
create_macos_app() {
    print_status "Creating macOS app bundle..."
    
    # Get version from header
    VERSION=$(grep PROG_VER_STR src/ft2_header.h | cut -d'"' -f 2)
    RELEASE_MACOS_DIR="release/macos/"
    APP_DIR="${RELEASE_MACOS_DIR}ft2-clone.app/"
    
    # Create app bundle structure
    mkdir -p "${APP_DIR}Contents/MacOS"
    mkdir -p "${APP_DIR}Contents/Resources"
    mkdir -p "${APP_DIR}Contents/Frameworks"
    
    # Copy binary
    cp ft2-clone "${APP_DIR}Contents/MacOS/ft2-clone"
    
    # Create Info.plist
    cat >"${APP_DIR}Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>ft2-clone</string>
    <key>CFBundleIdentifier</key>
    <string>com.ft2clone.ft2-clone</string>
    <key>CFBundleName</key>
    <string>ft2-clone</string>
    <key>CFBundleVersion</key>
    <string>$VERSION</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.11</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>CFBundleIconFile</key>
    <string>ft2-clone</string>
</dict>
</plist>
EOF
    
    # Create icon from PNG if available
    if [ -f "src/gfxdata/icon/ft2-clone.png" ]; then
        print_status "Creating macOS icon..."
        # Create iconset directory
        ICONSET_DIR="${APP_DIR}Contents/Resources/ft2-clone.iconset"
        mkdir -p "$ICONSET_DIR"
        
        # Convert PNG to various icon sizes required by macOS
        if command -v sips >/dev/null 2>&1; then
            # Use sips (macOS built-in tool) to create icon sizes
            sips -z 16 16 "src/gfxdata/icon/ft2-clone.png" --out "$ICONSET_DIR/icon_16x16.png" >/dev/null 2>&1
            sips -z 32 32 "src/gfxdata/icon/ft2-clone.png" --out "$ICONSET_DIR/icon_16x16@2x.png" >/dev/null 2>&1
            sips -z 32 32 "src/gfxdata/icon/ft2-clone.png" --out "$ICONSET_DIR/icon_32x32.png" >/dev/null 2>&1
            sips -z 64 64 "src/gfxdata/icon/ft2-clone.png" --out "$ICONSET_DIR/icon_32x32@2x.png" >/dev/null 2>&1
            sips -z 128 128 "src/gfxdata/icon/ft2-clone.png" --out "$ICONSET_DIR/icon_128x128.png" >/dev/null 2>&1
            sips -z 256 256 "src/gfxdata/icon/ft2-clone.png" --out "$ICONSET_DIR/icon_128x128@2x.png" >/dev/null 2>&1
            sips -z 256 256 "src/gfxdata/icon/ft2-clone.png" --out "$ICONSET_DIR/icon_256x256.png" >/dev/null 2>&1
            sips -z 512 512 "src/gfxdata/icon/ft2-clone.png" --out "$ICONSET_DIR/icon_256x256@2x.png" >/dev/null 2>&1
            sips -z 512 512 "src/gfxdata/icon/ft2-clone.png" --out "$ICONSET_DIR/icon_512x512.png" >/dev/null 2>&1
            sips -z 1024 1024 "src/gfxdata/icon/ft2-clone.png" --out "$ICONSET_DIR/icon_512x512@2x.png" >/dev/null 2>&1
            
            # Create .icns file from iconset
            if command -v iconutil >/dev/null 2>&1; then
                iconutil -c icns "$ICONSET_DIR" -o "${APP_DIR}Contents/Resources/ft2-clone.icns"
                rm -rf "$ICONSET_DIR"
                print_success "Created macOS icon: ${APP_DIR}Contents/Resources/ft2-clone.icns"
            else
                print_warning "iconutil not found, keeping PNG icons"
            fi
        else
            print_warning "sips not found, copying PNG icon as-is"
            cp "src/gfxdata/icon/ft2-clone.png" "${APP_DIR}Contents/Resources/"
        fi
    else
        print_warning "No icon found at src/gfxdata/icon/ft2-clone.png"
    fi
    
    # Copy SDL2 framework if available
    if [ -d "/Library/Frameworks/SDL2.framework" ]; then
        cp -R "/Library/Frameworks/SDL2.framework" "${APP_DIR}Contents/Frameworks/"
    fi
    
    # Sign the app (optional)
    if command -v codesign >/dev/null 2>&1; then
        print_status "Signing app bundle..."
        codesign --force --deep --sign - "${APP_DIR}" 2>/dev/null || print_warning "Code signing failed (this is normal for development builds)"
    fi
    
    print_success "macOS app bundle created: $APP_DIR"
}

# Main build process
print_status "Starting build process..."

# Build with autotools
FEATURES=""
if [ "$NO_MIDI" = false ]; then
    FEATURES="$FEATURES MIDI"
fi
if [ "$NO_FLAC" = false ]; then
    FEATURES="$FEATURES FLAC"
fi
if [ "$DEBUG" = true ]; then
    FEATURES="$FEATURES DEBUG"
fi

build_autotools "autotools" "$FEATURES"

# Create platform-specific packages
if [ "$BUILD_LINUX" = true ]; then
    if [ "$PLATFORM" = "Linux" ]; then
        create_linux_appimage
    else
        print_warning "Linux AppImage can only be created on Linux systems"
    fi
fi

if [ "$BUILD_MACOS" = true ]; then
    if [ "$PLATFORM" = "Darwin" ]; then
        create_macos_app
    else
        print_warning "macOS app bundle can only be created on macOS systems"
    fi
fi

print_success "Release build completed!"
print_status "Build summary:"
if [ "$BUILD_LINUX" = true ] && [ "$PLATFORM" = "Linux" ]; then
    print_status "  - Linux AppImage: release/linux/"
fi
if [ "$BUILD_MACOS" = true ] && [ "$PLATFORM" = "Darwin" ]; then
    print_status "  - macOS app: release/macos/"
fi
print_status "  - Binary: ft2-clone"
