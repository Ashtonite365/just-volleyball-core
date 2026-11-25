#!/usr/bin/env bash






set -uo pipefail
MODE="$1"; VAR="${2:-on}"; SRC="${SRC:?set SRC to your Just Volleyball game folder}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
R="$(mktemp -d /tmp/jvpc.XXXX)"; cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
mkdir -p "$R/mods"
if [ "$VAR" != vanilla ]; then
  cp -r "$HERE/dist/game_dir/." "$R/"; cp "$HERE"/bin/windows/*.dll "$R/mods/bin/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
  sed -i 's|^\[autoload\]$|[autoload]\nJvPcProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
  [ "$VAR" = off ] && echo '{"enabled": false}' > "$R/mods/player_consts.json"
  if [ "$VAR" = edited ]; then mkdir -p "$R/mods/goofy"; echo '{"player_consts": {"SET_DUMP_RISE_TAP": 5.0, "BLOCK_TURN": 4.0, "SPIKE_TURN_FLOOR": 0.5}}' > "$R/mods/goofy/90-pc-perf.json"; fi
else
  printf '[autoload]\nJvPcProbe="*res://mods/_probe.gd"\n' > "$R/override.cfg"
fi
cp "$HERE/tests/realgame/probe_pconsts.gd" "$R/mods/_probe.gd"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
JV_PC_MODE="$MODE" JV_PC_VARIANT="$VAR" WINEDEBUG=-all timeout "${SECS:-300}" wine "$R/volleyball.exe" --headless > "$R/run.log" 2>&1
code=$?
echo "== $MODE $VAR exit=$code"; grep -E '^(PROBE|PASS|FAIL)|player.consts' "$R/run.log" | grep -v '^PROBE boot'
echo "script_errors=$(grep -c 'SCRIPT ERROR' "$R/run.log") errors=$(grep -c '^ERROR' "$R/run.log") run_dir=$R"
[ "$MODE" = lock ] && { grep -q '^FAIL' "$R/run.log" && exit 1; grep -q 'PROBE RESULT fails=0' "$R/run.log" || exit 1; }
exit 0
