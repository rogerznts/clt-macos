#!/bin/bash
# Build build/CLT.app: Gcenx Wine (x86-64, runs under Rosetta 2) + DXMT (D3D11 -> Metal) + patched winemac.so + CLT.exe.
#   packaging/build.sh                 full build (downloads and compiles what is missing under vendor/)
#   packaging/build.sh --exe <path>    same, taking the game executable from <path> (game update)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WINE_VER=11.21
DXMT_VER=v0.80
VENDOR="$ROOT/vendor"
APP="$ROOT/build/CLT.app"
EXE="$ROOT/references/CLT.exe"
[ "${1:-}" = "--exe" ] && EXE="$2"
[ -f "$EXE" ] || { echo "missing game executable: $EXE" >&2; exit 1; }
mkdir -p "$VENDOR/src"

# 1. Wine for macOS (Gcenx build of upstream wine-devel).
WINE_APP="$VENDOR/Wine Devel.app"
if [ ! -d "$WINE_APP" ]; then
	curl -fL -o "$VENDOR/wine.tar.xz" \
		"https://github.com/Gcenx/macOS_Wine_builds/releases/download/$WINE_VER/wine-devel-$WINE_VER-osx64.tar.xz"
	tar -xf "$VENDOR/wine.tar.xz" -C "$VENDOR"
fi

# 2. DXMT builtin dlls.
if [ ! -d "$VENDOR/$DXMT_VER" ]; then
	curl -fL -o "$VENDOR/dxmt.tar.gz" \
		"https://github.com/3Shain/dxmt/releases/download/$DXMT_VER/dxmt-$DXMT_VER-builtin.tar.gz"
	tar -xzf "$VENDOR/dxmt.tar.gz" -C "$VENDOR"
fi

# 3. winemac.so of the same Wine release, exporting macdrv_functions for DXMT (packaging/wine-patches/).
SRC="$VENDOR/src/wine-$WINE_VER"
B64="$VENDOR/src/build64"
if [ ! -d "$SRC" ]; then
	git clone -q --depth 1 --branch "wine-$WINE_VER" https://gitlab.winehq.org/wine/wine.git "$SRC"
fi
cp "$ROOT"/packaging/wine-patches/winemac.drv/dxmt_* "$SRC/dlls/winemac.drv/"
grep -q dxmt_export.c "$SRC/dlls/winemac.drv/Makefile.in" ||
	sed -i '' 's/^\tdisplay.c \\/\tdisplay.c \\\n\tdxmt_cocoa.m \\\n\tdxmt_export.c \\/' "$SRC/dlls/winemac.drv/Makefile.in"
grep -q dxmt_cocoa.m "$SRC/dlls/winemac.drv/Makefile.in" ||
	sed -i '' 's/^\tdxmt_export.c \\/\tdxmt_cocoa.m \\\n\tdxmt_export.c \\/' "$SRC/dlls/winemac.drv/Makefile.in"
if [ ! -f "$B64/Makefile" ]; then
	command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "needs: brew install mingw-w64 bison" >&2; exit 1; }
	mkdir -p "$B64"
	(cd "$B64" && arch -x86_64 "$SRC/configure" --enable-archs=x86_64 --without-x --without-freetype \
		--without-gnutls --without-dbus --without-usb --without-pcap --without-sdl --without-gstreamer \
		--without-cups --without-krb5 --without-netapi --without-gphoto --without-sane --without-v4l2 \
		--without-pulse --without-capi --without-opencl --without-pcsclite --without-inotify --without-vulkan \
		--disable-tests CC="clang -arch x86_64" OBJC="clang -arch x86_64" >configure.log 2>&1)
fi
(cd "$B64" && arch -x86_64 make -j"$(sysctl -n hw.ncpu)" dlls/winemac.drv/winemac.so >make.log 2>&1)

# 4. Assemble the bundle.
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/game"
cp -c -R "$WINE_APP/Contents/Resources/wine" "$APP/Contents/Resources/wine"
W="$APP/Contents/Resources/wine/lib/wine"
for f in d3d11 dxgi d3d10core winemetal nvapi64 nvngx; do cp "$VENDOR/$DXMT_VER/x86_64-windows/$f.dll" "$W/x86_64-windows/"; done
for f in d3d11 dxgi d3d10core winemetal; do cp "$VENDOR/$DXMT_VER/i386-windows/$f.dll" "$W/i386-windows/"; done
cp "$VENDOR/$DXMT_VER/x86_64-unix/winemetal.so" "$W/x86_64-unix/"
cp "$B64/dlls/winemac.drv/winemac.so" "$W/x86_64-unix/winemac.so"
install_name_tool -id winemac.so "$W/x86_64-unix/winemac.so" 2>/dev/null
codesign -f -s - "$W/x86_64-unix/winemac.so" 2>/dev/null
python3 "$ROOT/packaging/brand_loader.py" "$W/x86_64-unix/wine" CLT
cp -c "$EXE" "$APP/Contents/Resources/game/CLT.exe"
cp "$ROOT/packaging/Info.plist" "$APP/Contents/Info.plist"
cp "$ROOT/packaging/launcher.sh" "$APP/Contents/MacOS/CLT"
chmod +x "$APP/Contents/MacOS/CLT"
python3 "$ROOT/packaging/make_icon.py" "$EXE" "$APP/Contents/Resources/CLT.icns"
xattr -cr "$APP"
echo "built $APP"
