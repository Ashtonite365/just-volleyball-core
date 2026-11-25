#!/usr/bin/env bash

set -uo pipefail
SRC="${1:?set SRC to your Just Volleyball game folder}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
DLLS="${2:-$HERE/bin/windows}"
R="$(mktemp -d /tmp/jvmenumove.XXXX)"
echo "RUN_DIR=$R DLLS=$DLLS"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"

install_mods() {
  cp -r "$HERE/dist/game_dir/." "$R/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
  mkdir -p "$R/mods/bin"
  cp "$DLLS"/libjvm_*.dll "$R/mods/bin/"

  rm -f "$R/mods/bin/libjvm_textures"* "$R/mods/bin/libjvm_models"*
  cp "$HERE/tests/realgame/probe_menu_move.gd" "$R/mods/_probe.gd"
  sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
}
install_probe_only() {
  rm -rf "$R/mods" "$R/override.cfg"
  mkdir -p "$R/mods"
  cp "$HERE/tests/realgame/probe_menu_move.gd" "$R/mods/_probe.gd"
  printf '[autoload]\nJvProbe="*res://mods/_probe.gd"\n' > "$R/override.cfg"
}
run_one() {
  local name="$1"
  WINEDEBUG=-all timeout 240 wine "$R/volleyball.exe" --headless > "$R/run-$name.log" 2>&1 || true
  echo "======= $name ======"
  grep -E '^PROBE |SCRIPT ERROR|Parse Error' "$R/run-$name.log" | head -80
}

install_mods
echo "---- mods ON ----"
run_one on
install_probe_only
echo "---- mods OFF ----"
run_one off
echo "DONE logs in $R"
