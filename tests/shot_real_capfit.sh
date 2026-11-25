#!/usr/bin/env bash




set -uo pipefail
OUT="$(realpath -m "$1")"; PREFIX="${2:-shot}"; SIZE="${3:-1280x720}"
SRC="${SRC:?set SRC to your Just Volleyball game folder}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
DIST="${DIST:-$HERE/dist/game_dir}"
BIN="${BIN:-$HERE/bin/windows}"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvcapfit.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$DIST/." "$R/"
mkdir -p "$R/mods/bin"
cp "$BIN"/*.dll "$R/mods/bin/"

x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
cp "$HERE/tests/realgame/probe_capfit.gd" "$R/mods/_probe.gd"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
W=${SIZE%x*}; H=${SIZE#*x}
JV_PROBE_SIZE="$SIZE" JV_PROBE_OUT="$(winepath -w "$OUT" 2>/dev/null)" JV_PROBE_PREFIX="$PREFIX" \
  WINEDEBUG=-all timeout "${SECS:-600}" xvfb-run -a -s "-screen 0 ${W}x${H}x24" \
  wine "$R/volleyball.exe" --rendering-driver opengl3 > "$R/run.log" 2>&1
echo "exit=$? run_dir=$R prefix=$PREFIX"
grep -E '^PROBE|SCRIPT ERROR|ERROR|jv-mods:(models|textures)' "$R/run.log" | tee "$OUT/${PREFIX}-probe.log"
