#!/bin/bash
# Builds dist/OpenClaw.app and dist/OpenClaw-<version>-arm64.dmg from an already built
# Build_Release/openclaw. The app contains no files of the original game.
#
# Usage: Scripts/package_macos.sh <version>
# Requires: Homebrew SDL2 libraries the game was linked with, dylibbundler (brew install dylibbundler)

set -euo pipefail

VERSION="${1:?Usage: $0 <version>}"
VERSION="${VERSION#v}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RELEASE_DIR="$ROOT/Build_Release"
DIST_DIR="$ROOT/dist"
APP="$DIST_DIR/OpenClaw.app"
CONTENTS="$APP/Contents"
FRAMEWORKS="$CONTENTS/Frameworks"
DMG="$DIST_DIR/OpenClaw-$VERSION-arm64.dmg"

if [ ! -x "$RELEASE_DIR/openclaw" ]; then
    echo "Build_Release/openclaw not found, build the game first" >&2
    exit 1
fi

rm -rf "$DIST_DIR"
mkdir -p "$CONTENTS/MacOS" "$CONTENTS/Resources" "$FRAMEWORKS"

echo "Assemble OpenClaw.app ..."
cp "$RELEASE_DIR/openclaw" "$CONTENTS/MacOS/OpenClaw"
# ASSETS.ZIP is built from Build_Release/ASSETS, so the app always has the current assets
(cd "$RELEASE_DIR/ASSETS" && zip -qr -X "$CONTENTS/Resources/ASSETS.ZIP" .)
for file in clacon.ttf console02.tga config.template.xml SAVES.template.XML; do
    cp "$RELEASE_DIR/$file" "$CONTENTS/Resources/"
done

echo "Create the icon ..."
ICON_TMP="$(mktemp -d)"
ICONSET="$ICON_TMP/OpenClaw.iconset"
mkdir -p "$ICONSET"
sips -s format png "$ROOT/ClawLauncher/Resources/launcher_icon.ico" --out "$ICON_TMP/icon.png" > /dev/null
for size in 16 32 64 128 256 512; do
    sips -z $size $size "$ICON_TMP/icon.png" --out "$ICONSET/icon_${size}x${size}.png" > /dev/null
done
for size in 16 32 128 256; do
    cp "$ICONSET/icon_$((size * 2))x$((size * 2)).png" "$ICONSET/icon_${size}x${size}@2x.png"
done
rm "$ICONSET/icon_64x64.png"
iconutil -c icns "$ICONSET" -o "$CONTENTS/Resources/OpenClaw.icns"
rm -rf "$ICON_TMP"

echo "Bundle libraries ..."
# The build leaves rpaths to the static libwap and Box2D directories, dylibbundler would turn them
# into duplicate rpaths, which dyld refuses to load
for rpath in $(otool -l "$CONTENTS/MacOS/OpenClaw" | awk '/cmd LC_RPATH/ { getline; getline; print $2 }'); do
    install_name_tool -delete_rpath "$rpath" "$CONTENTS/MacOS/OpenClaw"
done
dylibbundler -of -b -x "$CONTENTS/MacOS/OpenClaw" -d "$FRAMEWORKS" -p @executable_path/../Frameworks/ > /dev/null 2>&1

# sdl2-compat loads SDL3 at runtime with dlopen (it looks for @loader_path/libSDL3.dylib),
# so dylibbundler can't see it.
SDL3_LIB="$(brew --prefix sdl3)/lib/libSDL3.0.dylib"
cp "$SDL3_LIB" "$FRAMEWORKS/libSDL3.dylib"
chmod u+w "$FRAMEWORKS/libSDL3.dylib"
install_name_tool -id @executable_path/../Frameworks/libSDL3.dylib "$FRAMEWORKS/libSDL3.dylib" 2> /dev/null
if otool -L "$FRAMEWORKS/libSDL3.dylib" | tail -n +2 | grep -q -e /opt/homebrew -e /usr/local; then
    echo "libSDL3 depends on other Homebrew libraries:" >&2
    otool -L "$FRAMEWORKS/libSDL3.dylib" >&2
    exit 1
fi

# Every binary in the app must only use system libraries and the bundled ones
for binary in "$CONTENTS/MacOS/OpenClaw" "$FRAMEWORKS"/*.dylib; do
    if otool -L "$binary" | tail -n +2 | grep -e /opt/homebrew -e /usr/local; then
        echo "$binary still refers to libraries outside the app" >&2
        exit 1
    fi
done

# The app runs on the oldest macOS all of its binaries were built for
MIN_MACOS="$(for binary in "$CONTENTS/MacOS/OpenClaw" "$FRAMEWORKS"/*.dylib; do
    vtool -show-build "$binary" 2> /dev/null | awk '/minos/ { print $2 }'
done | sort -t. -k1,1n -k2,2n | tail -n 1)"
echo "Minimum macOS: $MIN_MACOS"

sed -e "s/@VERSION@/$VERSION/g" -e "s/@MIN_MACOS@/$MIN_MACOS/g" \
    "$ROOT/Scripts/macos/Info.plist.in" > "$CONTENTS/Info.plist"
plutil -lint "$CONTENTS/Info.plist" > /dev/null

echo "Sign (ad-hoc) ..."
codesign --force --sign - "$FRAMEWORKS"/*.dylib 2> /dev/null
codesign --force --sign - "$APP" 2> /dev/null
codesign --verify --deep --strict "$APP"

echo "Create the disk image ..."
STAGING="$(mktemp -d)"
cp -R "$APP" "$STAGING/"
ln -s /Applications "$STAGING/Applications"
cp "$ROOT/Scripts/macos/ReadMe.txt" "$STAGING/Read Me.txt"
hdiutil create -quiet -volname "OpenClaw $VERSION" -srcfolder "$STAGING" -fs HFS+ -format UDZO -ov "$DMG"
rm -rf "$STAGING"

echo "Done: $DMG"
