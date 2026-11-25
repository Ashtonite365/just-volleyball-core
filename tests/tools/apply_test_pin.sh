#!/usr/bin/env bash




SRC="$1"; R="$2"
[ -f "$SRC/test_pck_sha256.txt" ] || exit 0
python3 - "$R/mods/pins.json" "$(tr -d ' \n\r' < "$SRC/test_pck_sha256.txt")" <<'PY'
import json, sys
p, h = sys.argv[1], sys.argv[2]
d = json.load(open(p))
for pin in d.get("pins", []): pin["pck_sha256"] = h
json.dump(d, open(p, "w"), indent=2)
PY
