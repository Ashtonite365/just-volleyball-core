#!/usr/bin/env bash


set -uo pipefail
MODE="$1"; GDC="${2:-}"; SRC="${3:?set SRC to your Just Volleyball game folder}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
R="$(mktemp -d /tmp/jvct.XXXX)"; cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
mkdir -p "$R/mods/cb"; cp "$HERE/tests/realgame/probe_consttokens.gd" "$R/mods/_probe.gd"; [ -n "$GDC" ] && cp "$GDC" "$R/mods/cb/player_patched.gdc"; [ -n "${HVGDC:-}" ] && cp "$HVGDC" "$R/mods/cb/hitbox_view_patched.gdc"
printf '[autoload_prepend]\nJvProbe="*res://mods/_probe.gd"\n' > "$R/override.cfg"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
JV_CB_MODE="$MODE" WINEDEBUG=-all timeout "${SECS:-240}" wine "$R/volleyball.exe" --headless > "$R/run.log" 2>&1
echo "== $MODE"; grep -E '^PROBE' "$R/run.log"; echo "script_errors=$(grep -c 'SCRIPT ERROR' "$R/run.log") errors=$(grep -c '^ERROR' "$R/run.log")"; echo "run_dir=$R"
