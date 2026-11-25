# CORE developer tools (API 1.3)

Three tools in `jvm_core`, all local to this PC, reachable only through `JVModAPI`:

| tool | capability | access | default |
|---|---|---|---|
| Frame profiler | `dev.profiler` | read | idle (no per-frame work) until a capture starts |
| Debugger (log levels, capability dump, our-node inspector, event trace, console) | `dev.debugger` | local_ui | off |
| Altered-player-script checker (player.consts) | `player.consts_check` | read | on (cheap; see below) |

Node: `/root/JVModGate/JVDevTools` (plus a `tail` child for end-of-phase timestamps).
Function reference: [CORE-API.md "Developer tools (1.3)"](CORE-API.md#developer-tools-13).

## Frame profiler

```gdscript
var api = Engine.get_singleton("JVModAPI")
api.register_mod("my_mod", "1.3", PackedStringArray(["dev.profiler"]))
api.profiler_start("my_mod", 2.0, "snow")          # from the next frame, 2 s
# ... 2 s later
var cap = api.profiler_last_capture("my_mod", true) # meta, summary, stages, worst, hitches, frames
print(api.profiler_hitches("my_mod").explained_by)
```

### Starting a capture

All of these flip the same switch (`JVDevTools.profiler_toggle`): idle -> start a capture of
`profiler_seconds` (2 s), capturing -> stop now and keep what was recorded. A one-line overlay
shows while it runs; the files land in `user://jv_profiler/` as usual.

* **Mods panel** (main menu -> Mods): the `dev tools: frame profiler` entry under the mod list.
  Its label shows `capturing` while one runs and the last file name afterwards.
* **Hotkey F6** (when `devtools.hotkeys` is on): press to start, press again to stop early.
* **Console** `prof [seconds]`, or the API `profiler_toggle` / `profiler_start` / `profiler_stop`.

Hotkeys (`mods/core/*.json` -> `devtools`):

| key | default | meaning |
|---|---|---|
| `hotkeys` | `"auto"` | `true` / `false` turn the profiler + console keys on / off. `"auto"` follows the build: on in a developer build (`scons ... jv_dev=yes`), off in a normal build. |
| `profiler_key` | `"F6"` | profiler start / stop. Any Godot key name (`"F6"`, `"KEY_F6"`, `"Pause"` ...). |
| `console_key` | `"F11"` | debugger console. |

F6 and F11 are free in the game (it uses F9 for the camera and F10 for its hitbox view) and in our
libraries (F7 shots debugger, F8 CORE status overlay). To rebind, put e.g.
`{"devtools": {"profiler_key": "F5"}}` in a file of your own in `mods/core/` (later files win);
an unknown key name falls back to the default with a warning in the log. `profiler_status`
reports `hotkeys`, `hotkeys_source` (`build` / `config`), `dev_build`, `profiler_key` and
`console_key`.

### What one frame contains

A frame is one main-loop iteration: from its first physics tick (or its first `_process` when no
physics tick ran) to the next iteration. The frames of a capture tile the capture window exactly.

| column | meaning |
|---|---|
| `frame_ms` | iteration length |
| `physics_ms`, `physics_ticks` | sum of the physics ticks (first to last `_physics_process`), how many |
| `process_ms` | first to last `_process` (the profiler's head node runs first, its tail node last) |
| `render_cpu_ms` | `RenderingServer.frame_pre_draw` to `frame_post_draw` |
| `other_ms` | the rest: input, audio, buffer swap / vsync, frame pacing |
| `mods_process_ms` / `mods_physics_ms` / `mods_other_ms` | our top-level hooks inside that phase |
| `game_process_ms` / `game_physics_ms` | phase minus our hooks: the game's scripts and engine nodes |
| `engine_process_ms`, `engine_physics_ms` | Godot's own `Performance` monitors for the same frame |
| `viewport_render_cpu_ms`, `viewport_render_gpu_ms` | `RenderingServer` measured render time of the root viewport (0 when the driver does not measure; GPU can lag a frame) |
| `frame_setup_cpu_ms`, `draw_calls`, `objects`, `nodes` | `RenderingServer` / `Performance` |
| `stages` | ms per hook stage in this frame (below) |

### Hook stages

Every library records its own hook timings (`src/common/jv_prof.hpp`, `JVM_PROF("name")`). The
core turns the recorders on in each library through the library node's `_jv_dev_tap` method when
a capture starts and collects them at the end; samples are placed in frames by timestamp (one
steady clock per process). Nested stages (`a.b` inside `a`) are included in their parent. A name
used by several libraries gets a `[library]` suffix (`log.write[loader]`).

| stage | where |
|---|---|
| `textures.process` (`.gate_sync`, `.net_tick`, `.scene_change`, `.rescan`, `.locker_pose`) | texture loader `_process` |
| `textures.rescan.*` (`apply_scene_cosmetics`, `migrate_sync_character`, `refresh_players`, `update_missing_ui`) | the 0.5 s rescan |
| `textures.cosmetics` (`.full` = slow path: whole-scene walk + compare + apply, `.clear`) | `apply_scene_cosmetics` |
| `textures.stage_apply` (`.court`, `.sky`, `.env`), `court_env.apply`, `court_env.load_glb`, `court_sky.apply` | picked stage apply |
| `textures.image_mips_compress` | mipmaps + S3TC compress of pack images |
| `textures.players`, `textures.character_sync`, `textures.member_data_tick` | per-player skins, locker character sync, lobby member data |
| `models.process`, `models.tab_process`, `loader.process` | model loader, locker model tab, unified loader |
| `goofy.process`, `goofy.physics`, `goofy.net.session_tick`, `goofy.net.host_sync_tick` | Goofy |
| `shots.process`, `shots.physics` | extra shots |
| `core.gate_process` (`core.gate.evaluate` (`.probes`), `core.gate.status` (`.build`, `.lib.<library>` = that library's `get_status()`, `.texture_sync`, `.signature`, `.write`)), `core.api_process` (`.poll_events`), `core.menu_ui_process`, `core.menu_perf_process`, `core.picker_process` | CORE |
| `log.write` | one log line: open on first use, write, `fflush` |

### Output

`user://jv_profiler/capture-<YYYYMMDD-HHMMSS>-<n>[-tag].json` (meta, summary, stages, worst, hitches,
frames) and `.csv` (one row per frame, the built-in columns then one column per stage).
Hitches are frames above `max(2 x median, median + 8 ms)`; `explained_by` ranks the stages that
are slower in hitch frames than in normal frames, and `intervals_ms` shows any period.

### Cost

Idle: the profiler nodes do not process; each hook is one branch on a plain bool. Capturing:
two timestamps per hook call plus one record (buffers reserved up front, freed after the
capture); the frame's own bookkeeping is a few `Performance` / `RenderingServer` reads.

## Debugger

Off by default (`devtools.debugger`, or `debug_set_enabled`).

* **Log levels per library tag** (`debug_set_log_level`): 0 debug, 1 info (default), 2 warnings only,
  3 off, -1 reset; `*` sets the default. Stored once in Engine meta `jv_log_levels`, which every
  library reads (`jvm::log_level`), so one call covers all four libraries. `jvm::debug()` lines only
  appear at level 0.
* **Capability status dump** (`debug_capabilities`): every capability with impl / mapped / planned
  and its live open flag, counts, access classes, registered mods with call / refusal counters,
  libraries with their status and profiler tap.
* **Our nodes only** (`debug_tree`, `debug_node`): `/root/JV*` nodes and `JVM_*` / `jvm_*` nodes we
  added to the running scene; game nodes are refused.
* **Event trace** (`debug_trace`, 512 kept, only while on): event bus events, refused API calls,
  profiler start / done, player.consts findings, match start / end.
* **Console** (`debug_console_toggle`, or F11 with hotkeys on): `help`, `status`, `caps [filter]`, `mods`,
  `libs`, `log <tag> <level>`, `levels`, `tree [path] [depth]`, `node <path>`, `trace [n]`, `prof [s]`,
  `last`, `worst [n]`, `hitches`, `consts`, `audit`, `check <NAME> <value>`, `dump`.
* `debug_dump` writes all of the above to `user://jv_profiler/debug-dump-<time>.json`.

## player.consts checker

The player.consts bypass turns 34 gameplay consts of the game's player script into live static
vars (design: docs/PLAYER-CONSTS.md on the bypass branch). **Rule: the 34 values are frozen for
the whole match.** They may only be set before match start; the host saves them and distributes
them to every player.

`consts_check_write(mod, name, value)` judges a write before it happens; the bypass setter calls it,
mods may too. An accepted value becomes the expected live value.

| code | severity | when |
|---|---|---|
| `MID_MATCH_WRITE` | error | any write while a match runs (online `Net.in_match`, or the main match scene with a rally judge, CPU matches included) |
| `CAP_UNAVAILABLE` | error | bypass not mounted / not verified (reason given), or the gameplay class is closed (public, unknown): fail closed |
| `CONST_NOT_STATIC` | error | the name is still a const in the loaded script (the write would be ignored) |
| `PROTOCOL_NAME` | error | never-change names: `NET_FLOATS`, `CALLOUT_*` ids, `QUICK_CHAT`, `QUICK_CHAT_ACTIONS`, `CALL_VARIANTS`, `CALLOUT_GESTURES`, `CALL_SLACK_MS`, `HIGH_FIVE_ON` |
| `NOT_GAMEPLAY` / `UNKNOWN_NAME` | error | a visual const, or not one of the 34 |
| `NON_HOST_WRITE` | error | a client writes in a LAN / private / friends-only session |
| `BAD_TYPE` / `BAD_VALUE` / `OUT_OF_RANGE` | error | not a number, fraction on the int name, NaN / inf, outside the clamp (from the bypass; else sign kept and at most 4x stock) |

The audit (`consts_audit`; also at match start and once a second while the bypass is active, every
10 s otherwise) checks the live values:

| code | severity | when |
|---|---|---|
| `MID_MATCH_CHANGE` | error | a live value differs from the start-of-match set captured when the match began |
| `HOST_MISMATCH` | error | client: a live value differs from the host's start-of-match set (`consts_set_host_values`), or the host set names something that is not a bypassed gameplay name |
| `UNTRACKED_WRITE` | error | a live value changed without going through `consts_check_write` |
| `NOT_STOCK_WHILE_CLOSED` | error | not stock while the class is closed or the bypass is unavailable |
| `PROTOCOL_REWRITTEN` | error | a never-change name became a static (other scripts keep folded copies, listed) |
| `OUT_OF_RANGE` / `BAD_TYPE` / `BAD_VALUE` | error | live value outside clamp / type |
| `CONST_NOT_REWRITTEN` | warning | bypass active but a name is still const |
| `FOLDED_COPY_STALE` | warning | a changed name that another script reads as a folded literal (`BLOCK_TAPE_SLACK` in the debug hitbox view; `NET_FLOATS` in five scripts) |

Errors are logged once per (code, name, source) as `[jv-mods:consts] WARNING ...`, kept by
`consts_findings` with counts, and traced while the debugger is on. With the stock game (no
bypass) the audit reports nothing.

### Read API adapter (one function)

`JVDevTools::consts_live()` is the only place that talks to the bypass. It uses, in order:

1. `JVModAPI.player_consts_live()` (when the bypass is merged into CORE),
2. `/root/JVModGate/JVPlayerConsts.get_live_statics()`,
3. a direct read of the player script if the game already loaded it (it never loads it, so it cannot
   defeat the boot-time mount); this fallback knows no stock or clamp.

Expected shape:

```
{ available: bool, reason: String,
  statics: { NAME: { value, stock, min, max, type: "float" | "int" } },
  consts: PackedStringArray   # names still const
}
```

## Tests

* `JV_TEST_MODE=devtools tests/run_integration.sh <godot>`: profiler capture, files, per-library stages,
  frames tile the window, idle afterwards; debugger; checker with a synthetic player-script fixture
  and an adapter stand-in.
* `g++ -std=c++17 -Isrc tests/test_devtools.cpp src/gate/const_check.cpp -o /tmp/td && /tmp/td`: checker decisions.
* `tests/run_devtools_wine.sh <out> <packs>`: box-only CPU match on a picked stage with captures.
