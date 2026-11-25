#!/usr/bin/env bash


G="$1"; W="${2:-}"; cd "$(dirname "$0")/.."
fail=0; mkdir -p "${JV_LOG_DIR:-/tmp}"
[ -n "$W" ] && export JV_NET_WAIT="${JV_NET_WAIT:-150}" JV_TIMEOUT="${JV_TIMEOUT:-300}"
run_pair() {
  local name=$1; shift
  [ -n "${JV_PAIRS:-}" ] && [[ ",$JV_PAIRS," != *",$name,"* ]] && return
  ( env "${HOSTENV[@]}" JV_TEST_MODE=$1 tests/run_integration.sh "$G" $W > ${JV_LOG_DIR:-/tmp}/jvpair-$name-host.log 2>&1 ) & local hp=$!
  sleep "${JV_PAIR_DELAY:-4}"
  ( env "${CLIENTENV[@]}" JV_TEST_MODE=$2 tests/run_integration.sh "$G" $W > ${JV_LOG_DIR:-/tmp}/jvpair-$name-client.log 2>&1 ) & local cp=$!
  wait $hp $cp
  echo "##### $name"
  for f in ${JV_LOG_DIR:-/tmp}/jvpair-$name-host.log ${JV_LOG_DIR:-/tmp}/jvpair-$name-client.log; do
    grep -E 'FAIL|RESULT|PLAIN|sync:|content:|SCRIPT ERROR|mode=' "$f" | grep -v 'jv-mods:pin'
    grep -q 'RESULT [0-9]* passed, 0 failed' "$f" || fail=1
  done
}
HOSTENV=(JV_EXPECT_MISSING= JV_EXPECT_OTHER_SKIN=1 JV_EXPECT_CHANNEL=host_relay)
CLIENTENV=("JV_EXPECT_MISSING=You're missing BEEF from Hosty" JV_EXPECT_OTHER_SKIN=0 JV_EXPECT_CHANNEL=host_relay)
run_pair modded-host_modded-client nethost netclient
HOSTENV=()
CLIENTENV=(JV_EXPECT_MISSING= JV_EXPECT_OTHER_SKIN=0 JV_EXPECT_CHANNEL=lobby_member_data)
run_pair unmodded-host_modded-client plainhost netclient
HOSTENV=(JV_EXPECT_MISSING= JV_EXPECT_OTHER_SKIN=0 JV_EXPECT_CHANNEL=host_relay)
CLIENTENV=()
run_pair modded-host_unmodded-client nethost plainclient

HOSTENV=(JV_NET_LOBBY=private_then_public JV_EXPECT_MISSING= JV_EXPECT_OTHER_SKIN=1 JV_EXPECT_CHANNEL=host_relay)
CLIENTENV=(JV_NET_LOBBY=private_then_public "JV_EXPECT_MISSING=You're missing BEEF from Hosty" JV_EXPECT_OTHER_SKIN=0 JV_EXPECT_CHANNEL=host_relay)
run_pair private-then-public nethost netclient

for L in public quick; do
  HOSTENV=(JV_NET_LOBBY=$L); CLIENTENV=(JV_NET_LOBBY=$L)
  run_pair $L-lobby nethost netclient
done


for L in lan private public; do
  HOSTENV=(JV_GOOFY_SYNC=1 JV_NET_LOBBY=$L); CLIENTENV=(JV_GOOFY_SYNC=1 JV_NET_LOBBY=$L)
  run_pair goofy-sync-$L nethost netclient
done

SHOTDIR="$PWD/out/curve-display-shots"; mkdir -p "$SHOTDIR"
HOSTENV=(JV_NET_PORT=47941 JV_CURVE_SHOTS="$SHOTDIR"); CLIENTENV=(JV_NET_PORT=47941 JV_CURVE_SHOTS="$SHOTDIR")
run_pair curve-display curvehost curveclient



PCH=(JV_PLAYER_CONSTS=1); PCC=(JV_PLAYER_CONSTS=1)
HOSTENV=("${PCH[@]}" JV_PC_EXPECT=apply JV_PC_HELLOS=1); CLIENTENV=("${PCC[@]}" JV_PC_EXPECT=apply)
run_pair pc-all-modded nethost netclient
HOSTENV=("${PCH[@]}" JV_PC_EXPECT=stock JV_PC_HELLOS=0 "JV_PC_NOTICE=Clienty1 (missing)"); CLIENTENV=()
run_pair pc-unmodded-client nethost plainclient
HOSTENV=("${PCH[@]}" JV_PC_EXPECT=stock JV_PC_HELLOS=1 "JV_PC_NOTICE=Clienty1 (missing)"); CLIENTENV=("${PCC[@]}" JV_PC_EXPECT=stock JV_PC_CONST=1)
run_pair pc-client-unavailable nethost netclient
HOSTENV=("${PCH[@]}" JV_PC_EXPECT=stock JV_PC_HELLOS=1 "JV_PC_NOTICE=Clienty1 (other game build: otherbuild, host standin)"); CLIENTENV=("${PCC[@]}" JV_PC_EXPECT=stock JV_PIN_NAME=otherbuild)
run_pair pc-other-build nethost netclient

run_trio() {
  local name=$1
  [ -n "${JV_PAIRS:-}" ] && [[ ",$JV_PAIRS," != *",$name,"* ]] && return
  ( env "${PCH[@]}" JV_PC_EXPECT=stock JV_PC_PEERS=2 JV_PC_HELLOS=1 "JV_PC_NOTICE=Clienty2 (missing)" JV_PC_NOT_NOTICE=Clienty1 JV_TEST_MODE=nethost tests/run_integration.sh "$G" $W > /tmp/jvpair-$name-host.log 2>&1 ) &
  sleep "${JV_PAIR_DELAY:-4}"
  ( env "${PCC[@]}" JV_PC_EXPECT=stock JV_TEST_MODE=netclient tests/run_integration.sh "$G" $W > /tmp/jvpair-$name-client.log 2>&1 ) &
  sleep 3
  ( JV_TEST_MODE=plainclient tests/run_integration.sh "$G" $W > /tmp/jvpair-$name-plain.log 2>&1 ) &
  wait
  echo "##### $name"
  for f in /tmp/jvpair-$name-host.log /tmp/jvpair-$name-client.log /tmp/jvpair-$name-plain.log; do
    grep -E 'FAIL|RESULT|PLAIN|SCRIPT ERROR|mode=' "$f" | grep -v 'jv-mods:pin'
    grep -q 'RESULT [0-9]* passed, 0 failed' "$f" || fail=1
  done
}
run_trio pc-trio-one-unmodded



if [ -z "${JV_PAIRS:-}" ] || [[ ",$JV_PAIRS," == *",content-share,"* ]]; then
  CW="$(mktemp -d /tmp/jvcontent.XXXX)"; CP=$(( ${JV_NET_PORT:-47911} + 1 ))
  python3 tests/tools/mock_assets_server.py "$CW/w" "$CP" > "$CW/server.log" 2>&1 & SRV=$!
  for i in $(seq 50); do [ -f "$CW/w/ready" ] && break; sleep 0.2; done
  HOSTENV=(JV_CONTENT=1 JV_CONTENT_SRC="$CW/w/src" JV_ASSETS_BASE="http://127.0.0.1:$CP/")
  CLIENTENV=(JV_CONTENT=1 JV_ASSETS_BASE="http://127.0.0.1:$CP/")
  run_pair content-share nethost netclient

  HOSTENV=(JV_CONTENT=1 JV_CONTENT_SRC="$CW/w/src" JV_ASSETS_BASE="http://127.0.0.1:$CP/" JV_MODEL_ROOT=resources/packs)
  run_pair content-share-merged nethost netclient
  echo "mock repo requests: $(tr '\n' ' ' < "$CW/w/requests.log")"
  kill $SRV 2>/dev/null; wait $SRV 2>/dev/null
fi
exit $fail
