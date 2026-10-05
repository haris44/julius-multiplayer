#!/usr/bin/env bash
# Builds the game for macOS (Apple Silicon) and makes dist/CaesarIII-Multijoueur-VERSION.dmg to send to a friend:
# the application with its libraries inside, a French readme and the source code (AGPL).
# The Caesar III data is NOT included: every player needs his own copy of the game (GOG, Steam).
# Usage: tools/package-mac.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
BUILD=build-dist
DIST=dist
VERSION="$(date +%Y-%m-%d)-$(git rev-parse --short HEAD)"
NAME="CaesarIII-Multijoueur-$VERSION"
STAGE="$DIST/$NAME"

# the disk is nearly full: a release build and the image take about 150 MB
FREE_MB=$(df -m . | awk 'NR == 2 { print $4 }')
if [ "$FREE_MB" -lt 500 ]; then
    echo "Not enough free disk space (${FREE_MB} MB)" >&2
    exit 1
fi

echo "== Release build"
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$BUILD" -j8 --target julius > /dev/null

echo "== Libraries inside the application"
# fixup_bundle copies SDL2, SDL2_mixer, libpng... into the bundle and relinks the binary
cmake --install "$BUILD" > /dev/null
APP="$BUILD/julius.app"
# Homebrew's SDL2 is sdl2-compat, which loads SDL3 at run time (dlopen): fixup_bundle does not see it
SDL3=$(brew --prefix sdl3 2>/dev/null)/lib/libSDL3.0.dylib
if strings "$APP/Contents/Frameworks/libSDL2-2.0.0.dylib" | grep -q "libSDL3.dylib"; then
    [ -f "$SDL3" ] || { echo "SDL3 not found ($SDL3)" >&2; exit 1; }
    cp "$SDL3" "$APP/Contents/Frameworks/libSDL3.dylib"
    chmod u+w "$APP/Contents/Frameworks/libSDL3.dylib"
    install_name_tool -id @rpath/libSDL3.dylib "$APP/Contents/Frameworks/libSDL3.dylib" 2> /dev/null
fi
if otool -L "$APP/Contents/MacOS/julius" | grep -q "/opt/homebrew\|/usr/local"; then
    echo "The application still depends on Homebrew libraries:" >&2
    otool -L "$APP/Contents/MacOS/julius" >&2
    exit 1
fi
for lib in "$APP"/Contents/Frameworks/*.dylib; do
    if otool -L "$lib" | tail -n +2 | grep -v "$(basename "$lib")" | grep -q "/opt/homebrew\|/usr/local"; then
        echo "$lib still depends on Homebrew libraries" >&2
        exit 1
    fi
done
# relinking broke the signatures: sign everything again (ad hoc, no Apple developer account)
codesign --force --deep --sign - "$APP" > /dev/null 2>&1
codesign --verify --deep --strict "$APP"

echo "== Image contents"
rm -rf "$DIST"
mkdir -p "$STAGE"
ditto "$APP" "$STAGE/Caesar III Multijoueur.app"
cp tools/dist/LISEZMOI.txt "$STAGE/LISEZMOI.txt"
git archive --format=zip --prefix="julius-caesar-mp-$VERSION/" -o "$STAGE/code-source.zip" HEAD

echo "== Disk image"
hdiutil create -volname "Caesar III Multijoueur" -srcfolder "$STAGE" -ov -format UDZO "$DIST/$NAME.dmg" > /dev/null
rm -rf "$STAGE"
ls -lh "$DIST/$NAME.dmg"
