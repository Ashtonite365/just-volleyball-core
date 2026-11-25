#!/usr/bin/env bash




set -uo pipefail
OUT="$(realpath -m "$1")"; SIZE="${2:-1280x720}"; SRC="${SRC:?set SRC to your Just Volleyball game folder}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvrealin.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$HERE/dist/game_dir/." "$R/"; cp "${DLL_DIR:-$HERE/bin/windows}"/*.dll "$R/mods/bin/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
if [ -n "${EXTRA_MODEL:-}" ]; then mkdir -p "$R/mods/models/zz_test_model"; cp "$EXTRA_MODEL" "$R/mods/models/zz_test_model/"
  echo "{\"kind\":\"model\",\"id\":\"7E51\",\"name\":\"Test model\",\"model\":\"$(basename "$EXTRA_MODEL")\",\"game_hands\":\"tint\"}" > "$R/mods/models/zz_test_model/skin.json"; fi
cp "$HERE/tests/realgame/${PROBE:-probe_realinput}.gd" "$R/mods/_probe.gd"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
W=${SIZE%x*}; H=${SIZE#*x}
JV_PARTS="${PARTS:-}" JV_YAWS="${YAWS:-}" JV_TRACE="${TRACE:-}" JV_CASES="${CASES:-}" JV_PROBE_SIZE="$SIZE" JV_PROBE_OUT="$(winepath -w "$OUT" 2>/dev/null)" \
  WINEDEBUG=-all timeout "${SECS:-600}" xvfb-run -a -s "-screen 0 ${W}x${H}x24" \
  wine "$R/volleyball.exe" --rendering-driver opengl3 > "$R/run.log" 2>&1
echo "exit=$? run_dir=$R"
grep -E "^PROBE|SCRIPT ERROR|jv-mods:shots" "$R/run.log" | tee "$OUT/probe.log"
grep -q "^PROBE RESULT ALL PASS" "$OUT/probe.log" || { echo "REAL-INPUT PROBE FAILED"; exit 1; }
echo "REAL-INPUT PROBE: ALL PASS"
