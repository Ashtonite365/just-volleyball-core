# How jv-mods gets into the retail game (and why)

## The problem
An exported Godot 4 game only auto-loads GDExtensions listed in
`res://.godot/extension_list.cfg` inside its own `.pck`. just volleyball ships
`volleyball.exe` + a separate `volleyball.pck` (171 MB) + `steam_api64.dll`, and
was built without our extensions, so nothing loads our DLLs by itself.

## Options considered
| Option | Verdict |
|---|---|
| Edit/repack `volleyball.pck` (add extension_list.cfg / project.binary) | Rejected: modifies game files; Steam "verify files" and every update undo it. |
| `--main-pack` / `--script` launch options | Rejected: Godot 4.6+ release templates disable these by default (godot PR #111909). |
| Proxy DLL (e.g. fake `version.dll`/`winmm.dll`, or wrapping `steam_api64.dll`) | Rejected for now: native code injection, can't easily call Godot APIs without the engine's GDExtension handshake, looks like cheat software, breaks on updates. |
| **`override.cfg` next to the exe + autoload bootstrap + `GDExtensionManager.load_extension()`** | **Recommended.** Uses Godot's own documented override mechanism; adds files, changes none; uninstall = delete `override.cfg` and `mods\`. |

## Recommended approach (scaffolded in dist/game_dir/)
```
the game folder (the folder that contains volleyball.exe)/
  volleyball.exe              (untouched)
  volleyball.pck              (untouched)
  steam_api64.dll             (untouched)
  override.cfg                NEW - the only file outside mods\ ; autoloads res://mods/bootstrap.gd
  mods/                       NEW - the single jv-mods install folder
    bootstrap.gd              loads the extensions, spawns nodes
    pins.json                 approved game versions
    gate_probes.json          how the gate reads lobby state
    texture_sync.json         texture sync settings
    Watch-JvModStatus.ps1     live status viewer
    README.txt
    bin/*.gdextension + *.dll the five libraries
    resources/packs/ models/ shots/ goofy/   your mod content
```
1. Godot reads `override.cfg` beside the exe at startup.
   * Godot 4.6+: `[autoload_prepend]` (godot PR #113078) — bootstrap runs first.
   * Godot <= 4.5: use `[autoload]` instead (appended after the game's autoloads).
2. `res://mods/bootstrap.gd` is not in the .pck (the game's pck has no `mods/` folder), so Godot falls back to the real
   file next to the exe (same trick Godot Mod Loader uses).
3. bootstrap calls `GDExtensionManager.load_extension()` for core, then each category,
   and adds one node per library under `/root`.

Verification:
* **Real game**: a copy of volleyball.exe (Godot 4.7.1.stable.custom_build) + its
  .pck, run headless under Wine with a stub steam_api64.dll
  (tests/run_real_game_wine.sh): override.cfg `[autoload_prepend]` is honoured, the
  res:// -> exe-folder fallback works, all five Windows DLLs load, and the jv_net probe
  reads the real Net/GameConfig autoloads ("OFF: offline/menu"). The exe also contains
  the `disable_project_settings_override` string, but project.godot does not set it,
  so overrides are live in this build. Not tested: an actual match/lobby (needs Steam).
* Official Godot 4.5.2 + 4.7.2 Linux runtimes with a stand-in project
  (tests/run_integration.sh, both generic and real-game jv_net probes): 31/31 and 33/33.
* Official Godot 4.7.2 **Windows** binary under Wine with the Windows DLLs
  (`tests/run_all_linux.sh <Godot_win64_console.exe> win`).

## Caveats
* **Can be switched off by the dev in a future update**: Godot 4.5+ has
  `application/config/disable_project_settings_override` (PR #108818) and a
  `disable_overrides` build option. If the game sets either, override.cfg is ignored
  and this route is dead (editing the pck or exe is not an option for this project).
* **GDScript must be present** in the export (it almost always is) for bootstrap.gd.
* **Steam updates** don't delete extra files, but a game update can change node
  paths/properties the gate probes and mods rely on -> probes stop matching -> gate
  fails closed (mods off), which is the safe direction. Rebuild only needed if the
  engine minor version drops below 4.5.
* **Online / anti-cheat**: no anti-cheat found, but the game has a ban backend
  (`Mod` / `Backend` autoloads: lobby bans, chat bans) and matches are
  player-hosted with host migration, so client-side physics/shot/time-scale changes
  will desync or look like cheating to other players; the dev could ban/report.
  That is why the gate only allows LAN and private / friends-only lobbies, and why it
  fails closed. Textures are cosmetic and local-only but are gated too (by request).
* Everything runs inside the game process; a crash in a mod DLL crashes the game.

## Version pin (runs before any mod library is loaded)
`bootstrap.gd` loads only `jvm_core`, calls `JVModGate.verify_version()`, and loads the
category libraries **only if** `GameConfig.VERSION` (the game's own title/menu label,
`const VERSION` in `res://game/core/game_config.gd`) is on the approved list in
`mods/pins.json`: the top-level `versions` array and/or each pin's `version` field.
If the version string cannot be read, or is not approved, nothing else is loaded.
Steam buildid (from `../../appmanifest_5037200.acf`, read-only) and pck/exe size +
SHA256 are still recorded in `user://jv_mods/status.json` and the log, as info only.
The SHA256 is cached in `user://jv_mods/pin_cache.json` keyed by size+mtime.
Old `pins.json` files with no version field still match by pck hash/size (a readable
version string is still required). Missing/empty/unparseable pins file = refuse.

On mismatch: no category library is loaded (textures included), every library is
reported `disabled-version-mismatch`, an amber on-screen line says
"just volleyball mods disabled: game version changed (expected 1.0.4.2, found ...).
Update the mods." and the reason goes to `user://jv_mods/jv_mods.log`. The game runs
unmodded. jv-mods only ever writes inside `user://jv_mods/` (never the game's saves in
`user://data`).


## Status readout
`user://jv_mods/status.json` = on Windows
`%APPDATA%\Godot\app_userdata\volleyball\jv_mods\status.json`
(e.g. `C:\Users\<you>\AppData\Roaming\Godot\app_userdata\volleyball\jv_mods\status.json`).
Written atomically (temp file + `MoveFileExW` replace) the frame a stable field changes and at
most every 0.5 s otherwise. Per-frame diagnostics (for example `libraries.models.arms.max_stretch`)
do not count as a change and are not logged on every frame. Fields: `mode`, `gate.full/cosmetic {active, reason,
changed_frame}`, `libraries.{core,textures,shots,goofy,models} {state, reason}`
(state = active / blocked / disabled-version-mismatch / not-loaded), `texture_sync`,
`version_pin`, `timestamp` (local), `timestamp_utc`, `unix_ms`, `frame`.
Every stable change is also logged (`status: mode=...`). **F8** toggles an in-game overlay.
Live view: `powershell -ExecutionPolicy Bypass -File mods\Watch-JvModStatus.ps1` (shipped in the zip; source `tools/`).

The install folder is `mods\` next to the exe; the **user-data** folder stays
`user://jv_mods/` (`%APPDATA%\Godot\app_userdata\volleyball\jv_mods\`: log, status.json,
pin cache) - kept as is so it can't be confused with the game's own user data and
existing logs/caches stay valid.
