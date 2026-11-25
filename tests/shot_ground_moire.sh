#!/usr/bin/env bash





set -uo pipefail
OUT="$(realpath -m "${1:?out dir}")"
TAG="${2:?tag}"
MODE="${3:-stock}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$(realpath -m "${4:-$HERE/bin/windows}")"
SRC="${SRC:?set SRC to your Just Volleyball game folder}"
SIZE="${SIZE:-1280x720}"
mkdir -p "$OUT"
R="$(mktemp -d /tmp/jvmoire.XXXX)"
export WINEPREFIX="$R/winepfx"
export WINEARCH=win64
export WINEDEBUG=-all
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"

mkdir -p "$R/mods"
cp "$HERE/tests/realgame/probe_ground_moire.gd" "$R/mods/_probe.gd"
if [ "$MODE" != "off" ]; then
  cp -r "$HERE/dist/game_dir/." "$R/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
  mkdir -p "$R/mods/bin"
  cp "$BIN"/*.dll "$R/mods/bin/"

  cp "$HERE/tests/realgame/probe_ground_moire.gd" "$R/mods/_probe.gd"
  if [ "$MODE" = "royal" ]; then
    mkdir -p "$R/mods/resources/packs"
    printf '%s\n' '{"stage":"1005","ball":""}' > "$R/mods/resources/packs/picker.json"
  fi
fi
if [ -f "$R/override.cfg" ]; then
  sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
else
  printf '%s\n' '[autoload]' 'JvProbe="*res://mods/_probe.gd"' > "$R/override.cfg"
fi
W=${SIZE%x*}; H=${SIZE#*x}
JV_PROBE_SIZE="$SIZE" JV_PROBE_OUT="$(winepath -w "$OUT" 2>/dev/null || echo "$OUT")" \
  JV_MOIRE_MODE="$MODE" JV_MOIRE_TAG="$TAG" \
  timeout "${SECS:-240}" xvfb-run -a -s "-screen 0 ${W}x${H}x24" \
  wine "$R/volleyball.exe" --rendering-driver opengl3 > "$R/run.log" 2>&1
echo "exit=$? run_dir=$R tag=$TAG mode=$MODE"
grep -E '^PROBE|SCRIPT ERROR' "$R/run.log" | tee "$OUT/probe-$TAG.log" || true

python3 - "$OUT" "$TAG" << 'PY'
import sys
from pathlib import Path
from PIL import Image
out = Path(sys.argv[1]); tag = sys.argv[2]
for p in sorted(out.glob(f"*-{tag}.png")):
    if p.name.startswith("crop-"):
        continue
    im = Image.open(p)
    w, h = im.size

    crop = im.crop((int(w*0.08), int(h*0.38), int(w*0.92), int(h*0.92)))
    dest = out / f"crop-{p.name}"
    crop.save(dest)
    print(f"crop {dest} {crop.size} from {p.name} {im.size}")
PY
echo "run_dir=$R out=$OUT"
