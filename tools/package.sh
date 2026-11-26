#!/usr/bin/env bash

set -e
cd "$(dirname "$0")/.."
OUT=out/jv-mods-windows
rm -rf "$OUT"; mkdir -p "$OUT"
cp -r dist/game_dir/. "$OUT/"
cp bin/windows/libjvm_core*.dll "$OUT/mods/bin/"
cp tools/Watch-JvModStatus.ps1 "$OUT/mods/"
(cd out && rm -f jv-mods-windows.zip && python3 -c "import shutil; shutil.make_archive('jv-mods-windows', 'zip', '.', 'jv-mods-windows')")
sha256sum "$OUT"/mods/bin/*.dll out/jv-mods-windows.zip
