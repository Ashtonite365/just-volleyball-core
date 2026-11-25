#!/usr/bin/env bash




set -uo pipefail
OUT="$(realpath -m "$1")"; SIZE="${2:-1280x720}"; SRC="${SRC:?set SRC to your Just Volleyball game folder}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
AERO="${AERO:?set AERO to a player-model source folder}"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvmodels.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$HERE/dist/game_dir/." "$R/"; cp "$HERE"/bin/windows/*.dll "$R/mods/bin/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
P="$R/mods/${JV_MODEL_ROOT:-resources/packs}"
pack() {
  mkdir -p "$P/$1"; cp "$3" "$P/$1/"
  echo "{\"kind\":\"model\",\"id\":\"$2\",\"name\":\"$1\",\"model\":\"$(basename "$3")\"$4}" > "$P/$1/skin.json"
}
echo "--- model sources (sha256, mtime) right before the run"
for f in "$AERO/placeholder/aero_bean.glb" "$AERO/placeholder/aero_bean.tscn" "$AERO/assets/aero_player.glb" "$AERO/assets/aero_player.tscn"; do
  echo "$(sha256sum "$f" | cut -c1-64)  $(stat -c '%y' "$f")  $f"
done | tee "$OUT/model_hashes.txt"
pack "aero bean glb" AE0B "$AERO/placeholder/aero_bean.glb" ',"game_hands":"tint"'
pack "aero bean tscn" AE1B "$AERO/placeholder/aero_bean.tscn" ',"game_hands":"tint","tint_light":{"sss_tint":0.42}'
pack "aero player glb" AE0A "$AERO/assets/aero_player.glb" ',"game_hands":"tint"'
pack "aero player tscn" AE1A "$AERO/assets/aero_player.tscn" ',"game_hands":"tint","tint_light":{"sss_tint":0.42}'
cp "$HERE/tests/realgame/probe_locker.gd" "$R/mods/_probe.gd"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
W=${SIZE%x*}; H=${SIZE#*x}
JV_PROBE_SIZE="$SIZE" JV_PROBE_OUT="$(winepath -w "$OUT" 2>/dev/null)" JV_LOCKER_MODEL="${MODEL:-AE1B:bean_tscn}" JV_LOCKER_MODEL2="${MODEL2:-AE0B:bean_glb}" \
  WINEDEBUG=-all timeout "${SECS:-900}" xvfb-run -a -s "-screen 0 ${W}x${H}x24" \
  wine "$R/volleyball.exe" --rendering-driver opengl3 > "$R/run.log" 2>&1
echo "exit=$? run_dir=$R"
grep -E '^PROBE|SCRIPT ERROR|jv-mods:models' "$R/run.log" | tee "$OUT/probe.log"
