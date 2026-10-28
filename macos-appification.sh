#!/usr/bin/env bash
#
# macos-appification.sh
#
# Package an existing ft2-clone ARM64 build into a macOS application.
# Reuses the official macOS bundle metadata and icon.
#
# Requirements:
#   - macOS / Apple Silicon
#   - ft2-clone compiled using CMake
#   - Homebrew SDL2 or sdl2-compat
#
# The resulting application retains its Homebrew dependencies.
#

set -euo pipefail

# ------------------------------------------------------------------------------
# Configuration
# ------------------------------------------------------------------------------

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

APP_NAME="FastTracker"
APP_PATH="${ROOT_DIR}/${APP_NAME}.app"

TEMPLATE="${ROOT_DIR}/release/macos/ft2-clone-macos.app"
BINARY_SOURCE="${ROOT_DIR}/release/other/ft2-clone"

CONTENTS="${APP_PATH}/Contents"
BINARY_DEST="${CONTENTS}/MacOS/ft2-clone-macos"
PLIST="${CONTENTS}/Info.plist"

# ------------------------------------------------------------------------------
# Helpers
# ------------------------------------------------------------------------------

log() {
    printf '\n==> %s\n' "$*"
}

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

# ------------------------------------------------------------------------------
# Preflight checks
# ------------------------------------------------------------------------------

log "Checking environment..."

[[ "$(uname -s)" == "Darwin" ]] ||
    die "macOS is required."

[[ "$(uname -m)" == "arm64" ]] ||
    die "Apple Silicon is required."

[[ -x "$BINARY_SOURCE" ]] ||
    die "Missing compiled executable: $BINARY_SOURCE"

[[ -f "$TEMPLATE/Contents/Info.plist" ]] ||
    die "Missing official macOS application template."

[[ -f "$TEMPLATE/Contents/Resources/ft2-clone-macos.icns" ]] ||
    die "Missing official application icon."

file "$BINARY_SOURCE"

# ------------------------------------------------------------------------------
# Create application bundle
# ------------------------------------------------------------------------------

log "Creating ${APP_NAME}.app..."

rm -rf -- "$APP_PATH"

# Copy the official bundle while preserving its structure.
ditto "$TEMPLATE" "$APP_PATH"

# Remove bundled framework and signatures from the template.
# The executable uses Homebrew's SDL2 library instead.
rm -rf \
    "${CONTENTS}/Frameworks" \
    "${CONTENTS}/_CodeSignature"

mkdir -p "${CONTENTS}/MacOS"

# Copy the ARM64 executable using the name expected by Info.plist.
cp "$BINARY_SOURCE" "$BINARY_DEST"
chmod 755 "$BINARY_DEST"

# ------------------------------------------------------------------------------
# Validate Info.plist
# ------------------------------------------------------------------------------

log "Validating application metadata..."

plutil -lint "$PLIST"

/usr/libexec/PlistBuddy \
    -c "Set :CFBundleExecutable ft2-clone-macos" \
    "$PLIST"

/usr/libexec/PlistBuddy \
    -c "Set :CFBundleDisplayName FastTracker" \
    "$PLIST"

/usr/libexec/PlistBuddy \
    -c "Set :CFBundleName FastTracker" \
    "$PLIST"

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

log "Checking dynamic dependencies..."

otool -L "$BINARY_DEST"

# ------------------------------------------------------------------------------
# Code signing
# ------------------------------------------------------------------------------

log "Signing executable..."

codesign --force --sign - "$BINARY_DEST"

log "Signing application bundle..."

codesign --force --sign - "$APP_PATH"

# ------------------------------------------------------------------------------
# Verification
# ------------------------------------------------------------------------------

log "Verifying signature..."

codesign \
    --verify \
    --deep \
    --strict \
    --verbose=2 \
    "$APP_PATH"

# ------------------------------------------------------------------------------
# Result
# ------------------------------------------------------------------------------

log "Application successfully created!"

printf '\n'
printf 'Application: %s\n' "$APP_PATH"
printf 'Architecture: ARM64\n'
printf 'Icon:         Official ft2-clone icon\n'
printf 'SDL:          Homebrew dependency\n'
printf '\n'

printf 'Launch:\n'
printf '  open "%s"\n' "$APP_PATH"

printf '\nInstall:\n'
printf '  ditto "%s" "/Applications/%s.app"\n' \
    "$APP_PATH" "$APP_NAME"

