# just-volleyball-core

Unofficial fan-made core / loader and CORE API for Just Volleyball.

**This is an unofficial fan project.** It is not affiliated with, endorsed by, or supported by the developers of Just Volleyball. Everything published here is original work. Do not open issues with the official game team about this mod, and do not add game files (the executable, `.pck`, or any extracted official assets) to this repository. The loader is runtime-only: it does not edit the game's executable or `.pck`.

Related repositories: [mods](https://github.com/Ashtonite365/just-volleyball-mods) · [textures and models](https://github.com/Ashtonite365/just-volleyball-assets)


Four C++ GDExtension libraries (the texture and model loaders are merged into jvm_loader) (godot-cpp `godot-4.5-stable`, loads in Godot 4.5+;
the game is Godot 4.7.1 custom build + GodotSteam). Mods are pinned to the game's version string (currently approved: 1.0.4.2); Steam build id and pck hash are recorded as info only (docs/LOADING.md).

| library | class | tier | status |
|---|---|---|---|
| jvm_core | JVModGate (+JVMenuUI) | - | **working** version-string pin (approved game versions in pins.json), allowlist gate (fail closed), status.json, F8 overlay, logging, main-menu mods card/panel (docs/MENU.md), in-menu config editor |
| jvm_loader | JVLoader (+ JVTextureLoader, JVModelLoader subsystems) | cosmetic | **unified map loader** (docs/UNIFIED-LOADER.md): texture packs with 4-hex IDs, global replace, per-player skins, ID sync, optional fetch of missing packs from the assets repo (https only, hash + size checked, docs/CONTENT-SECURITY.md); custom player models from `mods/models` or `mods/resources/packs`; stage/ball picker packs; runs a stage pack's optional `goofy` section through Goofy's layer API when Goofy is installed + enabled. Replaces jvm_textures + jvm_models. |
| jvm_shots | JVExtraShots | full | **working on the real Ball** (struck hook); 9 shots as combos of the game's own actions + camera-relative curve (docs/SHOTS.md) |
| jvm_goofy | JVGoofyMods | full | **working** big ball, ball physics, player scale, max hits per team, no double-hit (+ Modifiers panel rows), time_scale opt-in (docs/GOOFY.md) |

Everything ON (local, no network) in the idle main menu too.
Everything ON: Free Play, practice, offline solo, LAN, friends-only and private lobbies.
Textures + models only, purely local (zero network): public custom lobbies, Quick Match, server browser. OFF: unknown.
See docs/GATE.md, docs/TEXTURE_SYNC.md, docs/CONTENT-SHARING.md (cosmetic sharing + assets repo downloads), docs/LOADING.md (loader, version pin, status).
CORE API function reference (every function's signature, parameters, return value, capability id, and a call example): [docs/API.md](docs/API.md). Gate rules: [rules.md](rules.md). Example script mod: `examples/api_mods/hello_api`.
Gameplay settings (CORE 1.3: mods register settings that change play; they appear in the game's Settings panel, host-synced, stock in public lobbies; a gameplay mod any player lacks is off for the whole match): docs/GAMEPLAY-SETTINGS.md. Goofy rules (double hit, touches in a row, team touches, classic block): docs/GOOFY-RULES.md.

**Status file (Windows):** `%APPDATA%\Godot\app_userdata\volleyball\jv_mods\status.json`
- watch it with `mods\Watch-JvModStatus.ps1` (repo: `tools/`); F8 in game toggles the overlay; F7 toggles the custom shot debugger (docs/SHOTS.md).
Log: `%APPDATA%\Godot\app_userdata\volleyball\jv_mods\jv_mods.log`.

## Install
Layout next to `volleyball.exe` - one folder plus one file:
```
override.cfg
mods\  bootstrap.gd  pins.json  gate_probes.json  texture_sync.json  Watch-JvModStatus.ps1  README.txt
       bin\  (5 .dll + 5 .gdextension)
       resources\packs\  models\  shots\  goofy\
```
1. Quit the game. If an older jv-mods build is installed, delete `override.cfg`,
   `jv_mods\` and `mods\` from the game folder first (back up your own
   texture packs / `mods\*\*.json` edits if you made any; old `mods\textures\` packs go into
   the `mods\resources\packs\` folder; the old `mods\resource packs\` is still read if the new one is missing).
2. Unzip `jv-mods-windows.zip` and copy the **contents** of its `jv-mods-windows\` folder
   (`override.cfg` and `mods\`) into the game folder (the folder that contains `volleyball.exe`).
3. Start the game from Steam. Optional live view:
   `powershell -ExecutionPolicy Bypass -File ".\mods\Watch-JvModStatus.ps1"` (run this from the game folder)
4. Check: log says `version pin: OK`, F8 shows the overlay; the main menu shows a
   full-size **mods** card (4th, in controller config's old slot) and a yellow **credits** button (movie icon) right
   of discord. Controls: docs/SHOTS.md (W walks normally; on a touch in the air: forward = topspin, back = Float,
   left/right = Curveball).
## Uninstall
Quit the game and delete `override.cfg` and `mods\` from the game folder (optionally
`%APPDATA%\Godot\app_userdata\volleyball\jv_mods\`). Nothing of the game was changed.

## Build
```
tools/fetch_godot_cpp.sh
scons platform=linux target=template_debug -j8                   # tests
scons platform=windows use_mingw=yes target=template_release -j8  # game DLLs
tools/package.sh                                                  # out/jv-mods-windows(.zip)
```
## Tests
```
g++ -std=c++17 tests/test_gate.cpp src/gate/*.cpp -o /tmp/t && /tmp/t   # unit
g++ -std=c++17 tests/test_api.cpp src/api/api_core.cpp -o /tmp/ta && /tmp/ta   # API unit (+ docs list every capability)
g++ -std=c++17 tests/test_gameplay_settings.cpp src/gate/*.cpp -o /tmp/tg && /tmp/tg  # gameplay settings registry + rules unit
tests/run_all_linux.sh <godot-linux-binary>                             # Godot runtime + net pairs
tests/run_all_linux.sh <Godot_win64_console.exe> win                    # Windows DLLs under Wine
tests/run_real_game_wine.sh <game-folder>                                # optional real-game test; pass your own game folder
```
