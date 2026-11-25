#!/usr/bin/env bash




set -uo pipefail
OUT="$(realpath -m "$1")"; SIZE="${2:-1280x720}"; SRC="${SRC:?set SRC to your Just Volleyball game folder}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvacc.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$HERE/dist/game_dir/." "$R/"; cp "$HERE"/bin/windows/*.dll "$R/mods/bin/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"

MESH="$HERE/tests/fixtures/models/bean.glb"
if [ -f "$MESH" ]; then
  mkdir -p "$R/mods/resources/packs/99_probe_custom_face"
  cp "$MESH" "$R/mods/resources/packs/99_probe_custom_face/face.glb"
  printf '%s\n' '{"id":"10F0","name":"Custom Face","kind":"skin","type":"face","category":"face","face":{"model":"face.glb","mount":"head"}}' \
    > "$R/mods/resources/packs/99_probe_custom_face/skin.json"
fi

python3 - "$R/mods/resources/packs/equipped.json" <<'PY'
import json,sys
p=sys.argv[1]
d=json.load(open(p))
eq=list(d.get("equipped") or [])
for i in ("1001","1002","1003","10F0"):
    if i not in eq: eq.append(i)
d["equipped"]=eq
json.dump(d, open(p,"w"), indent=2)
print("probe equipped.json", d)
PY
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
cp "$HERE/tests/realgame/probe_accessories.gd" "$R/mods/_probe.gd"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
W=${SIZE%x*}; H=${SIZE#*x}
JV_PROBE_SIZE="$SIZE" JV_PROBE_OUT="$(winepath -w "$OUT" 2>/dev/null)" \
  WINEDEBUG=-all timeout "${SECS:-900}" xvfb-run -a -s "-screen 0 ${W}x${H}x24" \
  wine "$R/volleyball.exe" --rendering-driver opengl3 > "$R/run.log" 2>&1
echo "exit=$? run_dir=$R"
grep -E '^PROBE|SCRIPT ERROR|jv-mods:textures' "$R/run.log" | tee "$OUT/probe.log"

if grep -q 'SCRIPT ERROR.*_probe\|SCRIPT ERROR.*jv' "$R/run.log"; then echo "FAIL script error"; exit 1; fi
if ! grep -q 'PROBE done' "$R/run.log"; then echo "FAIL probe did not finish"; exit 1; fi
if grep -q 'PROBE FAIL ' "$R/run.log"; then echo "FAIL probe assertions"; exit 1; fi
echo "ACCESSORIES PROBE: PASS"
