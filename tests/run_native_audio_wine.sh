#!/usr/bin/env bash




set -uo pipefail
OUT="$(realpath -m "${1:-/tmp/jv-native-audio-wine}")"
SRC="${SRC:?set SRC to your Just Volleyball game folder}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvaudio.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$HERE/dist/game_dir/." "$R/"
cp "$HERE"/bin/windows/*.dll "$R/mods/bin/"
"$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"

mkdir -p "$R/mods/resources/packs/10f8_audio_probe"
cat > "$R/mods/resources/packs/10f8_audio_probe/skin.json" <<'JSON'
{
  "id": "10F8",
  "type": "stage",
  "name": "Audio probe",
  "label": "Audio probe",
  "picker": true,
  "audio": { "ambience": "beach", "bus": "ambient", "volume": 1.0 }
}
JSON
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
cp "$HERE/tests/realgame/probe_native_audio.gd" "$R/mods/_probe.gd"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
WINEDEBUG=-all timeout "${SECS:-180}" wine "$R/volleyball.exe" --headless > "$R/run.log" 2>&1
echo "exit=$? run_dir=$R out=$OUT"
grep -E '^PROBE|SCRIPT ERROR|jv-mods:(pin|textures|loader)' "$R/run.log" | tee "$OUT/probe.log"
cp "$R/run.log" "$OUT/run.log"

fail=0
log="$OUT/probe.log"
grep -q 'PROBE api_version=1.4.0' "$log" || { echo "FAIL api version"; fail=1; }
grep -q 'PROBE idle_before=true' "$log" || { echo "FAIL not idle at boot"; fail=1; }
grep -q 'PROBE list_allocates_no_players=true' "$log" || { echo "FAIL list created players"; fail=1; }
grep -qE 'PROBE list n=(2[0-9]{2}|[3-9][0-9]{2,})' "$log" || { echo "FAIL unexpected list count"; fail=1; }
grep -q 'PROBE beach_found=true' "$log" || { echo "FAIL beach not found"; fail=1; }
grep -qE 'PROBE (apply_pack_handle=[1-9]|fallback_play_loop=[1-9])' "$log" || { echo "FAIL beach did not play"; fail=1; }
grep -q 'PROBE playing started=true' "$log" || { echo "FAIL not marked playing"; fail=1; }
grep -q 'PROBE after_stop idle=true' "$log" || { echo "FAIL did not return to idle"; fail=1; }
grep -q 'SCRIPT ERROR' "$R/run.log" && { echo "FAIL script error"; fail=1; }
[ $fail = 0 ] && echo "NATIVE AUDIO WINE: ALL PASS" || echo "NATIVE AUDIO WINE: FAILURES"
exit $fail
