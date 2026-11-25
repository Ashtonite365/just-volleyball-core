#!/usr/bin/env bash


set -euo pipefail
GODOT="${1:?godot binary}"; LABEL="${2:-run}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
export JV_TEST_MODE=perf JV_TIMEOUT=180

set +e
tests/run_integration.sh "$GODOT" > "/tmp/jv-perf-${LABEL}.full.log" 2>&1
rc=$?
set -e

RUN=$(grep -oE 'run_dir=/[^ ]+' "/tmp/jv-perf-${LABEL}.full.log" | tail -1 | cut -d= -f2)
if [ -n "$RUN" ] && [ -f "$RUN/run.log" ]; then
  cp "$RUN/run.log" "/tmp/jv-perf-${LABEL}.log"
  echo "==== PERF $LABEL (run_dir=$RUN) ===="
  grep -E '^PERF|^PASS|^FAIL|^RESULT|^SCRIPT ERROR' "$RUN/run.log" || true
else
  echo "==== PERF $LABEL (no run.log; integration rc=$rc) ===="
  tail -40 "/tmp/jv-perf-${LABEL}.full.log"
fi
exit $rc
