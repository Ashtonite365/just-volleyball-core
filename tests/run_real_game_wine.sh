#!/usr/bin/env bash





set -uo pipefail
SRC="${1:?set SRC to your Just Volleyball game folder}"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
R="$(mktemp -d /tmp/jvreal.XXXX)"
cp "$SRC/volleyball.exe" "$SRC/volleyball.pck" "$R/"
cp -r "$HERE/dist/game_dir/." "$R/"; cp "$HERE"/bin/windows/*.dll "$R/mods/bin/"; "$HERE/tests/tools/apply_test_pin.sh" "$SRC" "$R"
x86_64-w64-mingw32-objdump -p "$R/volleyball.exe" | awk '/DLL Name: steam_api64.dll/{f=1;next} /DLL Name:/{f=0} f && $NF ~ /^Steam/ {print $NF}' | sort -u |
  python3 -c "import sys; print('#include <stdint.h>'); [print('__declspec(dllexport) intptr_t %s(void){return %s;}' % (n, '1' if n in ('SteamInternal_SteamAPI_Init','SteamAPI_InitFlat') else '0')) for n in sys.stdin.read().split()]" > "$R/stub.c"
x86_64-w64-mingw32-gcc -shared -O1 -w -o "$R/steam_api64.dll" "$R/stub.c"
UD="$(winepath -u 'C:\users' 2>/dev/null)/$USER/AppData/Roaming/Godot/app_userdata/volleyball/jv_mods"
rm -f "$UD/pin_cache.json" "$UD/status.json" "$UD/jv_mods.log"
fail=0
run() {
  WINEDEBUG=-all timeout "${SECS:-60}" wine "$R/volleyball.exe" --headless > "$R/run-$1.log" 2>&1
  echo "--- $1"; grep -E 'jv-mods' "$R/run-$1.log" | grep -E 'pin|version|loaded|disabled|status:' | head -12
  local st; st=$(python3 -c "import json;d=json.load(open('$UD/status.json'));print(d['version_pin']['ok'], d['libraries']['textures']['state'], d['libraries']['shots']['state'], d['mode'])" 2>&1)
  echo "status.json: $st"
  case "$2" in
    ok)     [[ "$st" == True\ * ]] && ! [[ "$st" == *mismatch* ]] || { echo "FAIL $1"; fail=1; } ;;
    refuse) [[ "$st" == "False disabled-version-mismatch disabled-version-mismatch"* ]] || { echo "FAIL $1"; fail=1; } ;;
  esac
}
run cold ok
run warm ok

cp "$HERE/tests/realgame/probe.gd" "$R/mods/_probe.gd"; cp "$R/override.cfg" "$R/override.cfg.orig"
sed -i 's|^\[autoload\]$|[autoload]\nJvProbe="*res://mods/_probe.gd"|' "$R/override.cfg"
WINEDEBUG=-all timeout 150 wine "$R/volleyball.exe" --headless > "$R/run-probe.log" 2>&1
echo "--- probe"; grep -E '^PROBE|jv-mods:(menu|controls)\]|SCRIPT ERROR' "$R/run-probe.log" | head -40
for want in "mods_card=true" "credits_in_bar=true right_of_discord=true" "credits_in_tilt=false mods_in_tilt=true" "home_order=online,local,practice,mods,controller config,system settings,feedback & bug reports" "credits_class=Button text=credits icon=true yellow=true visible=true" "credits_stationary=true" "pad_nav=start:feedback & bug reports.* right>credits" "credits_opens=true" "mods_panel=true" "panel_modal=true modal_up=true" "behind_card_z=0 offset=false mouse_filter=2" "detail_view=goofy" "saved_big_ball=1.2 bak=true goofy_big_ball=1.2" "esc_back_to=list" "panel_closed=true modal_up_after=false focus=true" "move_forward_events=2 jvm_forward=false" "after_settle_forward_events=2" "goofy_max_hits_after_pick=4" "goofy_no_double_after_toggle=true" "credits_icon=res://game/ui_icon/Game Icons/PNG/Black/2x/movie.png not_discord_logo=true" "mods_card_h=74 practice_card_h=74" "validator_refused=true" "balls\\[menu-default\\] phys_radius=0.260 gravity=11.00 bounce=0.50 | Ball scale=1.000 visual_r=0.260" "balls\\[after-editor-big_ball-1.2\\] phys_radius=0.312 .*Ball scale=1.200 visual_r=0.312"; do


  if grep -q "PROBE fresh_button_focus=false" "$R/run-probe.log" && [[ "$want" == pad_nav=* || "$want" == *focus=true ]]; then
    echo "SKIP probe (env: no GUI focus under headless Wine, vanilla too): $want"; continue; fi
  grep -q "PROBE .*$want" "$R/run-probe.log" || { echo "FAIL probe: $want"; fail=1; }
done
grep -q 'SCRIPT ERROR.*jv\|SCRIPT ERROR.*_probe' "$R/run-probe.log" && { echo "FAIL probe: script error"; fail=1; }
grep -q 'InputMap action "jvm_' "$R/run-probe.log" && { echo "FAIL probe: polling a missing jvm_ action"; fail=1; }
mv "$R/override.cfg.orig" "$R/override.cfg"; rm -f "$R/mods/_probe.gd"
python3 - "$R/mods/pins.json" <<'PY'
import json,sys; p=sys.argv[1]; d=json.load(open(p)); d['pins'][0]['pck_sha256']='0'*64; json.dump(d,open(p,'w'))
PY
run tampered refuse
echo "run_dir=$R userdir=$UD"; [ $fail = 0 ] && echo "REAL GAME: ALL PASS" || echo "REAL GAME: FAILURES"
exit $fail
