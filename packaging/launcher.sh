#!/bin/bash
# CLT.app launcher: runs the unmodified CLT.exe under the bundled Wine (x86-64 via Rosetta 2) with DXMT (D3D11 -> Metal).
# The Wine prefix lives outside the bundle so the .app stays read-only and an updated CLT.exe is just a file swap.
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
RES="$HERE/Resources"
WINE_ROOT="$RES/wine"
SUPPORT="$HOME/Library/Application Support/CLT"
export WINEPREFIX="$SUPPORT/prefix"
export WINEDEBUG="${WINEDEBUG:--all}"
# d3d11=n,b: the frame-limiter proxy next to CLT.exe loads first and forwards to DXMT's builtin d3d11.
export WINEDLLOVERRIDES="d3d11=n,b;dxgi,d3d10core=b;winemenubuilder.exe=d"
export WINEMSYNC=1 WINEESYNC=1
export DXMT_LOG_PATH="$SUPPORT"
# Frame cap, applied by the d3d11 proxy right after each Present so input is read fresh (packaging/framelimiter/).
# Override: open --env CLT_MAX_FPS=90 CLT.app, or CLT_MAX_FPS=0 for uncapped.
export CLT_MAX_FPS="${CLT_MAX_FPS:-60}"
export PATH="$WINE_ROOT/bin:$PATH"
LOG="$SUPPORT/clt.log"
GAME_DIR="$WINEPREFIX/drive_c/Program Files/CLT"

if ! arch -x86_64 /usr/bin/true 2>/dev/null; then
	osascript -e 'display alert "CLT precisa do Rosetta 2" message "Instale com: softwareupdate --install-rosetta --agree-to-license"' >/dev/null
	exit 1
fi

mkdir -p "$SUPPORT"
if [ ! -f "$WINEPREFIX/system.reg" ]; then
	"$WINE_ROOT/bin/wineboot" --init >>"$LOG" 2>&1
	"$WINE_ROOT/bin/wineserver" -w
fi

# The exe stays in the bundle; the game dir in the prefix links to it so the game can write next to it
# (shader cache, settings). Replacing Resources/game/CLT.exe is the whole update procedure.
mkdir -p "$GAME_DIR"
ln -sfn "$RES/game/CLT.exe" "$GAME_DIR/CLT.exe"
ln -sfn "$RES/game/d3d11.dll" "$GAME_DIR/d3d11.dll"

cd "$GAME_DIR"
exec "$WINE_ROOT/bin/wine" "C:\\Program Files\\CLT\\CLT.exe" "$@" >>"$LOG" 2>&1
