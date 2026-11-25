#!/usr/bin/env bash

G="$1"; W="${2:-}"; cd "$(dirname "$0")/.."; rc=0

L="${JV_LOG_DIR:-/tmp}"; mkdir -p "$L"

g++ -std=c++17 -Isrc tests/test_devtools.cpp src/gate/const_check.cpp -o "$L/jv_test_devtools" && "$L/jv_test_devtools" | sed 's/^RESULT/devtools unit: RESULT/' | grep -E 'RESULT|FAIL' || rc=1

g++ -std=c++17 -Isrc tests/test_player_consts.cpp src/gate/player_consts.cpp -o "$L/jv_test_player_consts" && "$L/jv_test_player_consts" | sed 's/^RESULT/pconsts unit: RESULT/' | grep -E 'RESULT|FAIL' || rc=1
for m in generic api apistrict apicore apicore-goofy devtools gameplaysettings pconsts models jvnet gameplay menuui menuperf pickers loader loader-nogoofy pinfail-size pinfail-hash pinfail-missing; do
  JV_TEST_MODE=$m tests/run_integration.sh "$G" $W > $L/jvall-$m.log 2>&1 || rc=1
  echo "$m: $(grep -E '^RESULT' $L/jvall-$m.log | tail -1)"; grep '^FAIL' $L/jvall-$m.log
done

for m in generic gameplay gameplaysettings models pickers menuui menuperf loader jvnet; do
  JV_STRICT=1 JV_TEST_MODE=$m tests/run_integration.sh "$G" $W > $L/jvall-strict-$m.log 2>&1 || rc=1
  echo "strict $m: $(grep -E '^RESULT' $L/jvall-strict-$m.log | tail -1)"; grep '^FAIL' $L/jvall-strict-$m.log
done
tests/run_net_pairs.sh "$G" $W || rc=1
exit $rc
