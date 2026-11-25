#!/usr/bin/env bash





set -uo pipefail
GODOT="$1"; WIN="${2:-}"
MODE="${JV_TEST_MODE:-generic}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
RUN="$(mktemp -d /tmp/jvtest.XXXX)"
cp -r "$HERE/tests/fakegame/." "$RUN/"
sed -i "s/config\/name=\"fakegame\"/config\/name=\"fakegame-$(basename "$RUN")\"/" "$RUN/project.godot"
P() { if [ -n "$WIN" ]; then echo "Z:$1"; else echo "$1"; fi; }
if [ "$MODE" != plainhost ] && [ "$MODE" != plainclient ]; then
  mkdir -p "$RUN/mods/bin"
  cp "$HERE/dist/game_dir/mods/bootstrap.gd" "$RUN/mods/"
  cp "$HERE"/dist/game_dir/mods/bin/*.gdextension "$RUN/mods/bin/"
  if [ -n "$WIN" ]; then cp "$HERE"/bin/windows/*.dll "$RUN/mods/bin/"; else cp "$HERE"/bin/linux/*.so "$RUN/mods/bin/"; fi
  cp "$HERE/dist/game_dir/override.cfg" "$RUN/override.cfg"
fi
M="$RUN/_mods"; T="$M/resources/packs"; MD="$M/models"; mkdir -p "$T" "$M/shots" "$M/goofy"
mkp() { mkdir -p "$T/$1/res/tex"; cp "$HERE/tests/fixtures/$3.png" "$T/$1/res/tex/ball.png"; echo "{\"id\":\"$2\",\"name\":\"$1\",\"kind\":\"$4\"}" > "$T/$1/skin.json"; }
mkp ballpack BA11 red global
mkp "cafe skin pack" CAFE green skin

mkdir -p "$T/galaxy_ball" "$T/royal_court"
cp "$HERE/tests/fixtures/red.png" "$T/galaxy_ball/ball_tex.png"
cp "$HERE/tests/fixtures/red.png" "$T/galaxy_ball/preview.png"
echo '{"id":"1007","name":"Galaxy Tri-Color Ball","label":"Galaxy","type":"ball","preview":"preview.png","ball":{"albedo_tex":"ball_tex.png","albedo_color":"#ffffff","wrap":"equirect"}}' > "$T/galaxy_ball/skin.json"
cp "$HERE/tests/fixtures/green.png" "$T/royal_court/court_floor.png"
cp "$HERE/tests/fixtures/green.png" "$T/royal_court/preview.png"
echo '{"id":"1008","name":"Royal Court","label":"Royal Court","type":"stage","preview":"preview.png","base_stage":"grass","court":{"floor_tex":"court_floor.png"}}' > "$T/royal_court/skin.json"

mkdir -p "$T/env_ok_test" "$T/env_prop_test" "$T/env_bad_test"
cp "$HERE/tests/fixtures/green.png" "$T/env_ok_test/sky.png"
cp "$HERE/tests/fixtures/green.png" "$T/env_ok_test/preview.png"
cp "$HERE/tests/fixtures/green.png" "$T/env_ok_test/court_floor.png"
cp "$HERE/tests/fixtures/court_env/floor_deck.glb" "$T/env_ok_test/env.glb"
echo '{"id":"10E1","name":"Env Deck Test","label":"Env Deck","type":"stage","preview":"preview.png","base_stage":"grass","court":{"floor_tex":"court_floor.png"},"sky":{"panorama":"sky.png","energy":1.0,"ambient":"#6a88b0","fog_color":"#101018"},"environment":{"mesh":"env.glb","hide_env_props":true,"hide_ground":true,"baked_lighting":true}}' > "$T/env_ok_test/skin.json"
cp "$HERE/tests/fixtures/green.png" "$T/env_prop_test/preview.png"
cp "$HERE/tests/fixtures/green.png" "$T/env_prop_test/court_floor.png"
cp "$HERE/tests/fixtures/court_env/far_post.glb" "$T/env_prop_test/env.glb"
echo '{"id":"10E2","name":"Env Prop Test","label":"Env Prop","type":"stage","preview":"preview.png","base_stage":"grass","court":{"floor_tex":"court_floor.png"},"environment":{"mesh":"env.glb","hide_env_props":true,"hide_ground":false}}' > "$T/env_prop_test/skin.json"
cp "$HERE/tests/fixtures/green.png" "$T/env_bad_test/preview.png"
cp "$HERE/tests/fixtures/court_env/inside_post.glb" "$T/env_bad_test/env.glb"
echo '{"id":"10E3","name":"Env Bad Test","label":"Env Bad","type":"stage","preview":"preview.png","base_stage":"grass","court":{"floor_tex":"court_floor.png"},"environment":{"mesh":"env.glb","hide_env_props":true}}' > "$T/env_bad_test/skin.json"


mkdir -p "$T/env_missing_test"
cp "$T/env_ok_test/court_floor.png" "$T/env_ok_test/preview.png" "$T/env_missing_test/"
echo '{"id":"10E4","name":"Env Missing Mesh","label":"Env Missing","type":"stage","preview":"preview.png","base_stage":"grass","court":{"floor_tex":"court_floor.png"},"environment":{"mesh":"no_such_env.glb","hide_env_props":true,"hide_ground":true}}' > "$T/env_missing_test/skin.json"
mkdir -p "$T/cafe skin pack/add"; cp "$HERE/tests/fixtures/red.png" "$T/cafe skin pack/add/extra tex.png"

MESH="$HERE/tests/fixtures/models/bean.glb"
if [ -f "$MESH" ]; then
  mkdir -p "$T/champion_cap" "$T/flame_tail" "$T/custom_face"
  cp "$MESH" "$T/champion_cap/cap.glb"
  cp "$MESH" "$T/flame_tail/tail.glb"
  cp "$MESH" "$T/custom_face/face.glb"
  echo '{"id":"A100","name":"Champion Cap","kind":"skin","category":"head","hat":{"model":"cap.glb","mount":"head","hide_vanilla_hats":true}}' > "$T/champion_cap/skin.json"
  echo '{"id":"A103","name":"Flame Tail","kind":"skin","category":"extra","back":{"model":"tail.glb","mount":"chest","jiggle":true}}' > "$T/flame_tail/skin.json"
  echo '{"id":"A10F","name":"Custom Face","kind":"skin","type":"face","category":"face","face":{"model":"face.glb","mount":"head"}}' > "$T/custom_face/skin.json"
fi
mkp dupe ba11 green global
case "$MODE" in loader*)
  mkdir -p "$T/space_court" "$T/bad_goofy_court"
  cp "$HERE/tests/fixtures/green.png" "$T/space_court/court_floor.png"; cp "$HERE/tests/fixtures/green.png" "$T/bad_goofy_court/court_floor.png"
  echo '{"id":"1022","name":"Space Court","type":"stage","base_stage":"grass","court":{"floor_tex":"court_floor.png"},"goofy":{"version":1,"ball.gravity":{"mul":0.4}}}' > "$T/space_court/skin.json"
  echo '{"id":"1023","name":"Odd Goofy","type":"stage","base_stage":"grass","court":{"floor_tex":"court_floor.png"},"goofy":{"gravity_scale":0.5,"not.a.param":1}}' > "$T/bad_goofy_court/skin.json" ;;
esac
if [ "$MODE" = models ]; then
  F="$HERE/tests/fixtures/models"
  mkm() { mkdir -p "$MD/$1"; echo "$3" > "$MD/$1/skin.json"; [ -n "${4:-}" ] && cp -r $4 "$MD/$1/"; true; }
  mkm "aero glb" AE01 '{"kind":"model","id":"AE01","name":"bean glb","model":"bean.glb","game_hands":"tint"}' "$F/bean.glb"
  mkm "aero tscn" AE02 '{"kind":"model","id":"AE02","name":"bean tscn","model":"bean.tscn","tint_light":{"sss_tint":0.5}}' "$F/bean.tscn $F/shaders"
  mkm broken BAD1 '{"kind":"model","id":"BAD1","model":"corrupt.glb"}'; head -c 4000 /dev/urandom > "$MD/broken/corrupt.glb"
  mkm bones B0E5 '{"kind":"model","id":"B0E5","model":"badbones.glb"}' "$F/badbones.glb"
  mkm scripted 5C01 '{"kind":"model","id":"5C01","model":"scripted.tscn"}' "$F/scripted.tscn $F/scripts"
  mkm "scripted ok" 5C02 '{"kind":"model","id":"5C02","model":"scripted.tscn","allow_scripts":true}' "$F/scripted.tscn $F/scripts"
  mkm escape E5C0 '{"kind":"model","id":"E5C0","model":"../aero glb/bean.glb"}'
  mkm missing 4155 '{"kind":"model","id":"4155","model":"nope.glb"}'
  mkm "plump arms" BEA1 '{"kind":"model","id":"BEA1","name":"bean arms","model":"bean.glb","game_hands":"tint","arms":{"mode":"follow_hands","anchor_bone":"spine1","shoulder":[0.5,2.42,0.0],"upper":0.45,"lower":0.72,"radius":[0.15,0.16,0.155],"elbow_hint":[1.0,-0.5,-0.6]}}' "$F/bean.glb"
  mkm fixed F1C0 '{"kind":"model","id":"F1C0","model":"bean.glb","color":"#FF1A8C"}' "$F/bean.glb"
  mkm "skin in models" 5C1A '{"kind":"skin","id":"5C1A","name":"not a model"}'
fi
case "$MODE" in
  models) echo '{"equipped":["CAFE"]}' > "$T/equipped.json"; echo '{"equipped":["AE02"],"model":"AE01"}' > "$MD/equipped.json" ;;
  nethost) mkp beef BEEF green skin; echo '{"equipped":["BEEF"]}' > "$T/equipped.json"
    if [ -n "${JV_CONTENT_SRC:-}" ]; then
      cp -r "$JV_CONTENT_SRC"/packs/. "$T/"; mkdir -p "$MD"; cp -r "$JV_CONTENT_SRC"/models/. "$MD/"
      echo '{"equipped":["C001","C0DE","C0FF","C0EE"]}' > "$T/equipped.json"; echo '{"ball":"C0B1"}' > "$T/picker.json"; echo '{"model":"B0D1"}' > "$MD/equipped.json"
    fi ;;
  netclient) echo '{"equipped":["CAFE"]}' > "$T/equipped.json" ;;
  *) echo '{"equipped":["CAFE"]}' > "$T/equipped.json" ;;
esac
echo '{"backend":"rigid","ball":{"group":"ball"},"hitter":{"group":"hitter"},"shots":{"high_shot":{"key":"KEY_1"}}}' > "$M/shots/test.json"
echo '{"time_scale":0.5,"overrides":[{"target":{"group":"hitter"},"set":{"scale":[3,3,3]}}]}' > "$M/goofy/test.json"
if [ "$MODE" = gameplay ] || [ "$MODE" = perf ] || [ "$MODE" = menuperf ] || [ "$MODE" = curvehost ] || [ "$MODE" = curveclient ]; then
  rm -f "$M/shots/test.json" "$M/goofy/test.json"
  cp "$HERE/dist/game_dir/mods/shots/00-default.json" "$M/shots/"; cp "$HERE/dist/game_dir/mods/goofy/00-default.json" "$M/goofy/"
fi
if [ "$MODE" = gameplaysettings ]; then
  mkdir -p "$M/api_mods/low_grav"
  awk '/^```gdscript/{f=1;next} /^```/{if(f){exit}} f' "$HERE/docs/GAMEPLAY-SETTINGS.md" > "$M/api_mods/low_grav/mod.gd"
  echo '{"time_scale":0.5,"big_ball":1.5,"ball_physics":{"gravity_scale":0.8,"bounce_scale":1.2},"ball_params":{"flight.air_drag":0.3,"flight.curve_scale":{"mul":2}},"max_hits_per_team":4,"no_double_hit":true}' > "$M/goofy/test.json"
fi
[ "$MODE" = apistrict ] && echo '{"strict": true}' > "$M/api.json"
[ -n "${JV_STRICT:-}" ] && echo '{"strict": true}' > "$M/api.json"
case "$MODE" in apicore*) mkdir -p "$M/api_mods"; cp -r "$HERE/examples/api_mods/hello_api" "$M/api_mods/" ;; esac
echo '{"players_property":"_peer_players","local_player_property":"player","net":"/root/Net"}' > "$RUN/_sync.json"

[ "${JV_PC_CONST:-}" = 1 ] && sed -i 's/^static var /const /' "$RUN/game/player/player.gd"
if [ "$MODE" = pconsts ] || [ "$MODE" = generic ] || [ "$MODE" = models ] || [ "$MODE" = pickers ] || [ "$MODE" = api ] || [ "$MODE" = apistrict ] || [ "${MODE#apicore}" != "$MODE" ] || [ "${MODE#loader}" != "$MODE" ]; then
cat > "$RUN/_gate.json" <<'JSON'
{ "max_age_s": 2.0, "probes": [
  { "type": "node_property", "name": "fake-context", "authoritative": true, "node": "/root/Lobby", "property": "match_kind", "role": "context",
    "map": { "offline": "offline", "custom": "custom_lobby", "quick": "quick_match", "browser": "server_browser", "lan": "lan", "freeplay": "free_play" } },
  { "type": "node_property", "name": "fake-visibility", "authoritative": true, "node": "/root/Lobby", "property": "lobby_type", "role": "visibility",
    "map": { "0": "private", "1": "friends_only", "2": "public", "3": "invisible" } },
  { "type": "multiplayer_peer", "name": "net-peers" } ] }
JSON
else
  python3 - "$HERE/dist/game_dir/mods/gate_probes.json" "$RUN/_gate.json" <<'PY'
import json,sys; c=json.load(open(sys.argv[1])); c["probes"][0]["menu_scene"]="res://main.tscn"; json.dump(c,open(sys.argv[2],"w"))
PY
fi

if [ "$MODE" = pickers ]; then
  python3 "$HERE/tests/court_clearance.py" --self-check || { echo "FAIL clearance self-check"; exit 1; }
  python3 "$HERE/tests/court_clearance.py" "$T/royal_court" "$T/env_ok_test" "$T/env_prop_test" || { echo "FAIL clearance on legal fixture packs"; exit 1; }
  python3 "$HERE/tests/court_clearance.py" --expect-fail "$T/env_bad_test" || { echo "FAIL clearance did not reject intersecting 10E3"; exit 1; }
fi

head -c 3000000 /dev/urandom > "$RUN/fake.pck"
ENGINE=$( (if [ -n "$WIN" ]; then WINEDEBUG=-all timeout 60 wine "$GODOT" --version; else timeout 60 "$GODOT" --version; fi) 2>/dev/null | tr -d '\r' | tail -1)

ENGINE_STR=$(python3 -c "import sys; p=sys.argv[1].split('.'); v='.'.join(p[:3]) if p[2].isdigit() else '.'.join(p[:2]); i=3 if p[2].isdigit() else 2; print(f'{v}-{p[i]} ({p[i+1]})')" "$ENGINE")
SIZE=$(stat -c %s "$RUN/fake.pck"); SHA=$(sha256sum "$RUN/fake.pck" | cut -d' ' -f1)
case "$MODE" in
  pinfail-size) SIZE=$((SIZE+1));;
  pinfail-hash)
    case "${SHA:0:1}" in 0) SHA="f${SHA:1}";; *) SHA="0${SHA:1}";; esac;;
esac
echo "{\"pins\":[{\"name\":\"${JV_PIN_NAME:-standin}\",\"steam_buildid\":\"26924091\",\"engine\":\"$ENGINE_STR\",\"pck_size\":$SIZE,\"pck_sha256\":\"$SHA\"}]}" > "$RUN/_pins.json"
[ "$MODE" = pinfail-missing ] && rm -f "$RUN/_pins.json"
DRV="$MODE"; case "$MODE" in pinfail-*) DRV=pinfail;; loader*) DRV=loader;; esac





JV_GOOFY=""; JV_EXPECT_GOOFY=1
case "$MODE" in
  loader) JV_GOOFY=api; rm -f "$RUN/mods/bin/jvm_goofy.gdextension" ;;
  loader-nogoofy) JV_GOOFY=none; rm -f "$RUN/mods/bin/jvm_goofy.gdextension"; JV_EXPECT_GOOFY=0 ;;
  loader-oldgoofy) JV_GOOFY=old ;;
  loader-realgoofy) JV_GOOFY=real ;;
  apicore-goofy) JV_GOOFY=api; rm -f "$RUN/mods/bin/jvm_goofy.gdextension" ;;
esac
export JV_GOOFY JV_EXPECT_GOOFY


export JV_ASSETS_BASE="${JV_ASSETS_BASE:-http://127.0.0.1:9/}"
export JV_TEST_MODE="$DRV" JV_MODS_DIR="$(P "$M")" JV_GATE_CONFIG="$(P "$RUN/_gate.json")" JV_PINS_FILE="$(P "$RUN/_pins.json")" \
       JV_PCK_PATH="$(P "$RUN/fake.pck")" JV_TEXTURE_SYNC="$(P "$RUN/_sync.json")" WINEDEBUG=-all
run_godot() { if [ -n "$WIN" ]; then wine "$GODOT" "$@"; else "$GODOT" "$@"; fi; }
timeout 120 bash -c "$(declare -f run_godot); GODOT='$GODOT' WIN='$WIN' run_godot --headless --path '$(P "$RUN")' --import" > "$RUN/import.log" 2>&1
timeout "${JV_TIMEOUT:-150}" bash -c "$(declare -f run_godot); GODOT='$GODOT' WIN='$WIN' run_godot --headless --path '$(P "$RUN")'" > "$RUN/run.log" 2>&1
code=$?
grep -q "SCRIPT ERROR" "$RUN/run.log" && { echo "FAIL script error in driver"; code=1; }
grep -E 'PASS|FAIL|RESULT|PLAIN|SCRIPT ERROR|jv-mods:(pin|textures)\]' "$RUN/run.log" | grep -vE '^PASS' | head -60
echo "mode=$MODE engine='$ENGINE_STR' exit=$code passes=$(grep -c '^PASS' "$RUN/run.log") run_dir=$RUN"
exit $code
