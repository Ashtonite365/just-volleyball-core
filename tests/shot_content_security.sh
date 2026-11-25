#!/usr/bin/env bash



set -uo pipefail
OUT="$(realpath -m "${1:-/tmp/jv-content-security}")"; SRC="${SRC:?set SRC to your Just Volleyball game folder}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvcontentsec.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$HERE/dist/game_dir/." "$R/"; cp "$HERE"/bin/windows/*.dll "$R/mods/bin/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
if [ -n "${LIVE:-}" ]; then cp -r "$LIVE/resources/." "$R/mods/resources/"; cp -r "$LIVE/models/." "$R/mods/models/"; fi
python3 "$HERE/tests/tools/make_hostile_packs.py" "$R/mods" "res://mods" "$HERE/tests/fixtures" loader
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
cp "$HERE/tests/realgame/probe_content_security.gd" "$R/mods/_probe.gd"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
UD="$(winepath -u 'C:\users' 2>/dev/null)/$USER/AppData/Roaming/Godot/app_userdata/volleyball/jv_mods"
rm -f "$UD/jv_mods.log"
WINEDEBUG=-all timeout "${SECS:-240}" xvfb-run -a -s "-screen 0 1280x720x24" \
  wine "$R/volleyball.exe" --rendering-driver opengl3 --headless > "$R/run.log" 2>&1
echo "exit=$? run_dir=$R"
grep -E '^PROBE|SCRIPT ERROR|JVM_PWNED' "$R/run.log" | tee "$OUT/probe.log"
grep -E 'jv-mods:security' "$UD/jv_mods.log" > "$OUT/security.log" 2>/dev/null; cp "$UD/jv_mods.log" "$OUT/jv_mods.log" 2>/dev/null
fail=0
grep -q "PROBE pwned=0" "$OUT/probe.log" || { echo "FAIL canary ran"; fail=1; }
grep -q "JVM_PWNED" "$R/run.log" && { echo "FAIL canary printed"; fail=1; }
grep -q "PROBE after security_refused=[0-9]* pwned=0" "$OUT/probe.log" || { echo "FAIL canary ran later"; fail=1; }
grep -q "PROBE menu stage=.* env_applied=true" "$OUT/probe.log" || { echo "FAIL env GLB did not spawn"; fail=1; }

if grep 'refused:' "$OUT/security.log" | grep -v 'hostile_' | grep -q .; then echo "FAIL a real pack was refused:"; grep 'refused:' "$OUT/security.log" | grep -v 'hostile_'; fail=1; fi
for h in hostile_tres hostile_global hostile_spoof hostile_traversal hostile_bomb; do grep -q "$h" "$OUT/security.log" || { echo "FAIL $h not refused"; fail=1; }; done
grep -q 'SCRIPT ERROR.*jv\|SCRIPT ERROR.*_probe' "$R/run.log" && { echo "FAIL probe: script error"; fail=1; }
echo "run_dir=$R out=$OUT"
[ $fail = 0 ] && echo "CONTENT SECURITY PROBE: PASS" || echo "CONTENT SECURITY PROBE: FAILURES"
exit $fail
