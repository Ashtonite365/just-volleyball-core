#!/usr/bin/env bash



set -uo pipefail
OUT="$(realpath -m "$1")"; SIZE="${2:-1280x720}"; SRC="${SRC:?set SRC to your Just Volleyball game folder}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvpick.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$HERE/dist/game_dir/." "$R/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
cp "$HERE"/bin/windows/*.dll "$R/mods/bin/"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"

rm -f "$R/mods/resources/packs/picker.json"
cp "$HERE/tests/realgame/probe_pickers.gd" "$R/mods/_probe.gd"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
W=${SIZE%x*}; H=${SIZE#*x}
run_phase() {
  local phase="$1"
  JV_PROBE_SIZE="$SIZE" JV_PROBE_OUT="$(winepath -w "$OUT" 2>/dev/null)" JV_PROBE_PHASE="$phase" \
    JV_DROP_PACK="$(winepath -w "$HERE/tests/fixtures/sunrise_ball" 2>/dev/null)" \
    WINEDEBUG=-all timeout "${SECS:-420}" xvfb-run -a -s "-screen 0 ${W}x${H}x24" \
    wine "$R/volleyball.exe" --rendering-driver opengl3 > "$R/run-$phase.log" 2>&1
  echo "phase=$phase exit=$? run_dir=$R"
  grep -E '^PROBE|SCRIPT ERROR|jv-mods:(menu|textures)\]' "$R/run-$phase.log" | tee -a "$OUT/probe.log"
}
echo "=== phase 1: open pickers, select, drop-in pack, freeplay ==="
run_phase 1
echo "=== picker.json after phase 1 ==="
cat "$R/mods/resources/packs/picker.json" 2>/dev/null | tee -a "$OUT/probe.log" || echo "(missing picker.json)" | tee -a "$OUT/probe.log"
echo "=== phase 2: restart, confirm persist ==="
sleep 5
run_phase 2
if grep -q 'X Error' "$R/run-2.log" 2>/dev/null || ! grep -q 'PROBE done phase2' "$OUT/probe.log" 2>/dev/null; then
  echo "=== phase 2 retry after Wine+Xvfb flake ==="
  sleep 5
  run_phase 2
fi
ls -la "$OUT"
