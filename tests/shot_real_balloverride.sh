#!/usr/bin/env bash



set -uo pipefail
OUT="$(realpath -m "$1")"; SIZE="${2:-1280x720}"; SRC="${SRC:?set SRC to your Just Volleyball game folder}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvbo.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$HERE/dist/game_dir/." "$R/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
cp "$HERE"/bin/windows/*.dll "$R/mods/bin/"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
rm -f "$R/mods/resources/packs/picker.json"
P="$R/mods/resources/packs/90_goofy_test_court"; mkdir -p "$P"
cp "$HERE/tests/fixtures/packs/goofy_court/skin.json" "$P/"
cp "$HERE/tests/fixtures/green.png" "$P/preview.png"; cp "$HERE/tests/fixtures/green.png" "$P/court_floor.png"
cp "$HERE/tests/realgame/probe_balloverride.gd" "$R/mods/_probe.gd"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
W=${SIZE%x*}; H=${SIZE#*x}
JV_PROBE_OUT="$(winepath -w "$OUT" 2>/dev/null)" WINEDEBUG=-all timeout "${SECS:-420}" xvfb-run -a -s "-screen 0 ${W}x${H}x24" \
  wine "$R/volleyball.exe" --rendering-driver opengl3 > "$R/run.log" 2>&1
echo "exit=$? run_dir=$R"
grep -E '^PROBE|SCRIPT ERROR|jv-mods:(goofy|menu)\]' "$R/run.log" | tee "$OUT/probe.log"
ls -la "$OUT"
