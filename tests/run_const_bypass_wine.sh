#!/usr/bin/env bash



set -uo pipefail
FIX="${1:?patched player source fixture}"; SRC="${2:?set SRC to your Just Volleyball game folder}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
R="$(mktemp -d /tmp/jvcb.XXXX)"; cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
mkdir -p "$R/mods/cb"; cp "$HERE"/tests/realgame/cb/*.gd "$R/mods/cb/"; cp "$HERE/tests/realgame/probe_constbypass.gd" "$R/mods/_probe.gd"; cp "$FIX" "$R/mods/cb/_player_patched.txt"
printf '[autoload]\nJvProbe="*res://mods/_probe.gd"\n' > "$R/override.cfg"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
WINEDEBUG=-all timeout "${SECS:-120}" wine "$R/volleyball.exe" --headless > "$R/run.log" 2>&1
grep -E '^PROBE|SCRIPT ERROR|Parse Error|ERROR' "$R/run.log" | head -80; echo "run_dir=$R"
