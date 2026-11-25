#!/usr/bin/env bash




set -uo pipefail
OUT="$(realpath -m "$1")"; PACKS="$2"; SRC="${SRC:?set SRC to your Just Volleyball game folder}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"; SIZE="${SIZE:-1280x720}"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvdt.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$HERE/dist/game_dir/." "$R/"; cp "$HERE"/bin/windows/*.dll "$R/mods/bin/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
mkdir -p "$R/mods/resources/packs"; cp -r "$PACKS"/. "$R/mods/resources/packs/"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
cp "$HERE/tests/realgame/probe_devtools.gd" "$R/mods/_probe.gd"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
UD="$(winepath -u 'C:\users' 2>/dev/null)/$USER/AppData/Roaming/Godot/app_userdata/volleyball"
rm -rf "$UD/jv_profiler"
W=${SIZE%x*}; H=${SIZE#*x}
WINEDEBUG=-all timeout "${SECS:-420}" xvfb-run -a -s "-screen 0 ${W}x${H}x24" wine "$R/volleyball.exe" --rendering-driver opengl3 > "$R/run.log" 2>&1
echo "exit=$? run_dir=$R"
grep -E '^PROBE|SCRIPT ERROR|jv-mods:(devtools|consts)' "$R/run.log" | tee "$OUT/probe.log"
cp -r "$UD/jv_profiler" "$OUT/" 2>/dev/null; cp "$UD/jv_mods/jv_mods.log" "$OUT/" 2>/dev/null
echo "script_errors=$(grep -c 'SCRIPT ERROR' "$R/run.log") out=$OUT"
