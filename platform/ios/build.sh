#!/bin/bash
# Builds LostWorld.ipa for iPhone and iPad (unsigned: for LiveContainer, or to
# sign yourself with Sideloadly / AltStore / Xcode). On a Mac with Xcode,
# Python 3.10+ and CMake 3.20+ (brew install cmake ninja python):
#
#   platform/ios/build.sh [path/to/lostwsga.zip]
#
# The ROM is looked for as an argument, in LW_ROM_ZIP, then next to the
# repository's README. It is needed to build (the game's code is recompiled
# from it) and is packed into the app, so the game finds it without copying
# anything to the phone. LW_NO_ROM_IN_APP=1 leaves it out: put lostwsga.zip in
# the app's Documents folder (Files app) instead.
#
# Steps: the ROM extracted and its PowerPC code recompiled (as build.bat),
# glslang and m68kmake built for this Mac, MoltenVK downloaded, the game
# cross-compiled for arm64 iOS, and the app packed as out/ios/LostWorld.ipa.
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT=$(pwd)

ZIP=${1:-${LW_ROM_ZIP:-$ROOT/lostwsga.zip}}
MOLTENVK_VERSION=${MOLTENVK_VERSION:-v1.4.2}
BUNDLE_ID=${LW_BUNDLE_ID:-com.snicolast.lostworld}
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 8)

fail() { echo; echo "Build failed: $*"; exit 1; }

# ---- tools
xcrun --sdk iphoneos --show-sdk-path >/dev/null 2>&1 || fail "the iOS SDK was not found: install Xcode (App Store), open it once, then: sudo xcode-select -s /Applications/Xcode.app"
command -v cmake >/dev/null || fail "cmake not found (brew install cmake)"
PY=""
for p in python3.13 python3.12 python3.11 python3.10 python3; do
    if command -v "$p" >/dev/null && "$p" -c 'import sys; sys.exit(sys.version_info < (3, 10))' 2>/dev/null; then PY=$p; break; fi
done
[ -n "$PY" ] || fail "Python 3.10 or later not found (brew install python)"
GEN="Unix Makefiles"
command -v ninja >/dev/null && GEN=Ninja

# ---- [1/6] the ROM's data, [2/6] its code recompiled
if [ ! -f data/crom.bin ] || [ ! -f data/sound_samples.bin ]; then
    [ -f "$ZIP" ] || fail "ROM not found ($ZIP): give the path to lostwsga.zip as an argument or in LW_ROM_ZIP"
    echo "[1/6] Extracting the ROM"
    "$PY" tools/romtool.py "$ZIP" data
fi
echo "[2/6] Recompiling the PowerPC code"
"$PY" tools/recomp.py data/crom.bin build/gen

# ---- [3/6] glslang and m68kmake for this Mac (the build runs them)
echo "[3/6] Building the build tools for this Mac"
cmake -S . -B build/ios-host -G "$GEN" -DCMAKE_BUILD_TYPE=Release -DLW_HOST_TOOLS_ONLY=ON -DLW_TESTS=OFF >/dev/null
cmake --build build/ios-host --target host_tools -j "$JOBS"
HOST_GLSLANG=$(find build/ios-host -type f -name glslang -perm -u+x | head -1)
[ -n "$HOST_GLSLANG" ] || HOST_GLSLANG=$(find build/ios-host -type f -name 'glslang-standalone' -perm -u+x | head -1)
HOST_M68KMAKE=$(find build/ios-host -type f -name m68kmake -perm -u+x | head -1)
[ -n "$HOST_GLSLANG" ] && [ -n "$HOST_M68KMAKE" ] || fail "glslang or m68kmake was not built"

# ---- [4/6] MoltenVK (Vulkan on Metal), linked into the app
MVK_LIB=${MOLTENVK_LIB:-}
if [ -z "$MVK_LIB" ]; then
    MVK_DIR=third_party/downloads/MoltenVK-$MOLTENVK_VERSION
    MVK_LIB=$(find "$MVK_DIR" -path '*static*' -path '*ios-arm64/*' -name libMoltenVK.a 2>/dev/null | grep -v simulator | head -1 || true)
    if [ -z "$MVK_LIB" ]; then
        echo "[4/6] Downloading MoltenVK $MOLTENVK_VERSION"
        mkdir -p "$MVK_DIR"
        ok=""
        for asset in MoltenVK-ios.tar MoltenVK-all.tar; do
            url=https://github.com/KhronosGroup/MoltenVK/releases/download/$MOLTENVK_VERSION/$asset
            if curl -fL --retry 3 -o "$MVK_DIR/$asset" "$url"; then
                tar -xf "$MVK_DIR/$asset" -C "$MVK_DIR" && ok=1 && break
            fi
        done
        [ -n "$ok" ] || fail "MoltenVK could not be downloaded. Download MoltenVK-ios.tar (or MoltenVK-all.tar) from
  https://github.com/KhronosGroup/MoltenVK/releases into $MVK_DIR and run this again,
  or give the path of libMoltenVK.a (static, ios-arm64) in MOLTENVK_LIB"
        MVK_LIB=$(find "$MVK_DIR" -path '*static*' -path '*ios-arm64/*' -name libMoltenVK.a | grep -v simulator | head -1 || true)
    else
        echo "[4/6] MoltenVK: $MVK_LIB"
    fi
    [ -n "$MVK_LIB" ] || fail "no static ios-arm64 libMoltenVK.a in $MVK_DIR"
fi
MVK_LIB=$(cd "$(dirname "$MVK_LIB")" && pwd)/$(basename "$MVK_LIB")

# ---- [5/6] the game, for arm64 iOS
echo "[5/6] Building the game for iOS"
cmake -S . -B build/ios -G "$GEN" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
    -DLW_HOST_GLSLANG="$ROOT/$HOST_GLSLANG" -DLW_HOST_M68KMAKE="$ROOT/$HOST_M68KMAKE" \
    -DMOLTENVK_LIB="$MVK_LIB" -DLW_BUNDLE_ID="$BUNDLE_ID" -DLW_TESTS=OFF
cmake --build build/ios --target LostWorld -j "$JOBS"
APP=$(find build/ios -maxdepth 3 -type d -name LostWorld.app | head -1)
[ -n "$APP" ] || fail "LostWorld.app was not made"

# ---- [6/6] the app's files and the .ipa
echo "[6/6] Packing LostWorld.ipa"
STAGE=out/ios/Payload
rm -rf out/ios && mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
DEST=$STAGE/LostWorld.app/game
mkdir -p "$DEST/third_party"
cp -R res textures "$DEST/"
cp -R third_party/fonts "$DEST/third_party/"
rm -f "$DEST/res/lostworld.rc"
if [ -z "${LW_NO_ROM_IN_APP:-}" ] && [ -f "$ZIP" ]; then cp "$ZIP" "$DEST/lostwsga.zip"; fi
# The icon: the largest picture in res/lostworld.ico, at 120 and 180 pixels
"$PY" - res/lostworld.ico out/ios/icon.png <<'PYEOF'
import struct, sys
d = open(sys.argv[1], "rb").read()
n = struct.unpack_from("<H", d, 4)[0]
best = None
for i in range(n):
    w, h, _, _, _, _, size, off = struct.unpack_from("<BBBBHHII", d, 6 + 16 * i)
    w = w or 256
    if d[off:off + 8] == b"\x89PNG\r\n\x1a\n" and (best is None or w > best[0]):
        best = (w, d[off:off + size])
if best: open(sys.argv[2], "wb").write(best[1])
PYEOF
if [ -f out/ios/icon.png ]; then
    sips -z 120 120 out/ios/icon.png --out "$STAGE/LostWorld.app/AppIcon60x60@2x.png" >/dev/null
    sips -z 180 180 out/ios/icon.png --out "$STAGE/LostWorld.app/AppIcon60x60@3x.png" >/dev/null
fi
# Ad hoc signature (LiveContainer and the sideloading tools sign it again)
codesign --force --sign - --timestamp=none "$STAGE/LostWorld.app" >/dev/null 2>&1 || true
(cd out/ios && rm -f LostWorld.ipa && zip -qry LostWorld.ipa Payload)
echo
echo "Done: out/ios/LostWorld.ipa ($(du -h out/ios/LostWorld.ipa | cut -f1))"
echo "LiveContainer: send it to the phone (AirDrop, Files) and add it with LiveContainer's + button."
