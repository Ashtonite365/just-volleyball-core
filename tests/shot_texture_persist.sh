#!/usr/bin/env bash



set -uo pipefail
OUT="$(realpath -m "${1:-/tmp/jv-texture-persist}")"; SRC="${SRC:?set SRC to your Just Volleyball game folder}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvtexpersist.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$HERE/dist/game_dir/." "$R/"; cp "$HERE"/bin/windows/*.dll "$R/mods/bin/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
cp "$HERE/tests/realgame/probe_texture_persist.gd" "$R/mods/_probe.gd"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
WINEDEBUG=-all timeout "${SECS:-240}" xvfb-run -a -s "-screen 0 1280x720x24" \
  wine "$R/volleyball.exe" --rendering-driver opengl3 --headless > "$R/run.log" 2>&1
echo "exit=$? run_dir=$R"
grep -E '^PROBE|SCRIPT ERROR|jv-mods:textures' "$R/run.log" | tee "$OUT/probe.log"
fail=0
for want in "menu_cache_ready=true" "locker_no_reload=true" "locker_cache_same=true" "after_locker_no_reload=true" "after_locker_cache_same=true" "after_locker_still_loaded=true" "freeplay_no_reload=true" "freeplay_cache_same=true"; do
  grep -q "PROBE .*$want" "$OUT/probe.log" || { echo "FAIL probe: $want"; fail=1; }
done
grep -q 'SCRIPT ERROR.*jv\|SCRIPT ERROR.*_probe' "$R/run.log" && { echo "FAIL probe: script error"; fail=1; }
echo "run_dir=$R out=$OUT"
[ $fail = 0 ] && echo "TEXTURE PERSIST PROBE: PASS" || echo "TEXTURE PERSIST PROBE: FAILURES"
exit $fail
