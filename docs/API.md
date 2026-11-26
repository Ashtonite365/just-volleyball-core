# CORE API 1.3 — contract for mods

The CORE library (`jvm_core`) is the translation layer every mod hooks into. Mods name a
feature ("pick this court", "ball gravity ×0.5", "tell me when the mode changes"); CORE
finds it in the game or in a loaded library and checks every call against the gate.
When the game or a library changes, CORE is updated in one place and mods keep working.

This file is the function reference for CORE. It lists every API function: its signature,
what it does, its parameters and return value, the capability id it requires, and an
example of how to call it. For the Goofy library's own contract, see [GOOFY-API.md](GOOFY-API.md).

**This is an unofficial fan project.** It is not affiliated with, endorsed by, or supported
by the developers of Just Volleyball. The API is runtime-only: it does not edit the game's
executable or `.pck`.

## Where it lives

* Node `/root/JVModGate/JVModAPI` (class `JVModAPI`), also available as
  `Engine.get_singleton("JVModAPI")` from GDScript.
* C++: `jvm::find_api()` in `src/common/api_client.hpp`.
* The API lives in `jvm_core`, so it is present exactly when the core / gate is.

```gdscript
var api = Engine.get_singleton("JVModAPI")
var r = api.register_mod("my_mod", "1.3", PackedStringArray(["scene.balls", "ball.physics"]))
if not r.ok:
    push_error(r.reason)
    return
print(api.get_api_version())  # "1.3.0"
var balls = api.find_balls("my_mod")
```

```cpp
#include "common/api_client.hpp"
godot::Object *api = jvm::find_api();
godot::PackedStringArray caps;
caps.push_back("scene.balls");
api->call("register_mod", "my_mod", "1.3", caps);
```

## Versioning

* `get_api_version()` returns `"1.3.0"` (`major.minor.patch`).
* A mod registers with the version it was written for. Same major and a minor less than or
  equal to CORE's is accepted (`1`, `1.0`, `1.3` on 1.3.0). A higher minor or a different
  major is refused.
* Minor bumps only add functions, capabilities, or events; nothing listed here changes
  meaning within major 1. Each function lists the API version it appeared in.

## How calls are checked

* Every call that is about a specific mod takes that mod's name as the first argument.
* The call is checked against (1) the capabilities the mod declared in `register_mod` and
  (2) the gate rule of that capability's access class.
* Refused calls return a neutral value (`false`, empty, or `null`) and are logged once per
  `(mod, capability, reason)`.
* Undo calls (marked *undo* below) only need the capability declared, so a mod can always
  revert after its class has closed.
* If the core leaves, every class reports closed (`access_changed`) before the API goes away.
* If a subsystem is not installed, its functions return the neutral value; check
  `libraries_status`.

Capability ids come from `src/api/capabilities.def`. Each function section below names the
capability it requires (or states that none is needed). `get_capabilities()` returns the
full live table.

## Access classes and multiplayer safety

| access | open in | multiplayer |
|---|---|---|
| `read` | every mode once pinned | Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies. |
| `local_ui` | every mode once pinned | Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies. |
| `cosmetic` | every identified mode (incl. public) | Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context. |
| `gameplay` | Free Play, practice, offline solo, menu, LAN, private, friends-only | Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown. |
| `sync` | LAN, private, friends-only | Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies. |
| `offline` | Free Play, practice, offline solo, menu | Unsynced engine-wide change: open ONLY in Free Play / practice / offline solo / menu; refused in every online lobby (LAN/private/friends too) so nothing desyncs. |

Gate rules (rules.md): **Free Play / offline: everything. LAN / private / friends-only: full (gameplay + sync). Public / quick match / server browser: local-only (read, local_ui, cosmetic).** `gate_matrix()` returns the exact table.

## Contents (250 functions, 3 signals, 88 capabilities)

- [API info, registration, gate](#api-info-registration-gate) (18)
- [Config and state](#config-and-state) (9)
- [Events and signals](#events-and-signals) (8)
- [Scene, players, rally (game internals by meaning)](#scene-players-rally-game-internals-by-meaning) (10)
- [Lobby data (sync)](#lobby-data-sync) (3)
- [Ball](#ball) (11)
- [Rules and the Modifiers panel](#rules-and-the-modifiers-panel) (10)
- [Engine, overrides, generic escape hatch](#engine-overrides-generic-escape-hatch) (10)
- [Packs, textures, stage / ball picks](#packs-textures-stage--ball-picks) (14)
- [Courts: environment and sky](#courts-environment-and-sky) (4)
- [Native audio (1.4)](#native-audio-14) (11)
- [Accessories and player models](#accessories-and-player-models) (15)
- [Unified loader](#unified-loader) (6)
- [Goofy ball (all 156 params)](#goofy-ball-all-156-params) (19)
- [Player consts (1.2)](#player-consts-12) (4)
- [Goofy ball + player libraries, host sync (1.2)](#goofy-ball--player-libraries-host-sync-12) (13)
- [Gameplay settings registry and game rules (1.3)](#gameplay-settings-registry-and-game-rules-13) (26)
- [Shots and curve](#shots-and-curve) (11)
- [Library game IO](#library-game-io) (9)
- [Picker, Mods panel, menu perf](#picker-mods-panel-menu-perf) (13)
- [Developer tools (1.3)](#developer-tools-13) (26)
- [Signals](#signals) · [Events](#events) · [Properties](#properties) · [Config keys and files](#config-keys-and-files) · [Capabilities](#capabilities) · [Script mods and the example](#script-mods-and-the-example) · [Writing a C++ mod](#writing-a-c-mod)

## API info, registration, gate

Call these first. `register_mod` is required before any call that names a mod.

### `get_api_version()` → String

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** API version `major.minor.patch` (now `1.4.0`). No side effects.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.get_api_version()  # "1.4.0"`

### `register_mod(name: String, version: String, caps: PackedStringArray)` → Dictionary {ok, reason, granted, denied}

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Registers (or re-registers) a mod with the API version it was written for (same major, minor <= ours) and the capabilities it will call. Unknown and planned ids are denied (listed in `denied`). Logged once.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.register_mod("my_mod", "1.1", PackedStringArray(["scene.balls", "goofy.ball"]))`

### `get_capabilities()` → Array[Dictionary] {id, since, access, scope, status, feature, game_refs, functions, open}

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** The whole capability table with the live `open` flag.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `for c in api.get_capabilities(): print(c.id, c.open)`

### `has_capability(id: String)` → bool

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** True when the id exists and is not planned.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.has_capability("court.env")`

### `can(mod: String, cap: String)` → bool

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Would a call under `cap` by `mod` be allowed right now (declared + gate). Does not count as a call.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `if api.can("my_mod", "goofy.ball"): ...`

### `why_not(mod: String, cap: String)` → String

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Empty when allowed, else the reason ("offline only: ... (mode: LAN)").
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `print(api.why_not("my_mod", "goofy.ball"))`

### `is_open(access: String)` → bool

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Is an access class open now: read, local_ui, cosmetic, gameplay, sync, offline.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.is_open("offline")`

### `get_reason(access: String)` → String

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Human reason for the class state (gate text).
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.get_reason("gameplay")`

### `get_mode()` → String

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Detected play mode: Free Play, practice, offline solo, menu, LAN, private, friends-only, public, quick match, server browser, unknown.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.get_mode()`

### `gate_matrix()` → Dictionary {mode: {access: bool}}

- **Since** 1.1 · no capability needed (API metadata)
- **Does / side effects:** The full mode x access-class table the gate enforces (rules.md). Pure; no side effects.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.gate_matrix()["LAN"]["sync"]  # true`

### `net_safety(cap: String)` → Dictionary {found, access, scope, open_in, public_safe, reaches_other_players}

- **Since** 1.1 · no capability needed (API metadata)
- **Does / side effects:** Multiplayer profile of a capability: in which modes it opens and whether it reaches other players.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.net_safety("goofy.ball").open_in`

### `get_status()` → Dictionary {version, strict, mode, open, mods, coverage}

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Live API status (also summarised in status.json under `api`).
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.get_status()`

### `get_coverage()` → Dictionary {capabilities, impl, mapped, planned, game_refs, used_by_mods, by_access}

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Coverage numbers of the capability table.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.get_coverage()`

### `get_mods()` → Dictionary {mod: {version, declared, calls, refusals}}

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Registered mods with per-capability call / refusal counters.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.get_mods()["goofy"].declared`

### `library_allowed(short_name: String)` → bool

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Strict mode check used by bootstrap.gd (`mods/api.json {"strict": true}`): only libraries migrated to the API load.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.library_allowed("shots")`

### `is_strict()` → bool

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Strict API mode on/off (reads mods/api.json once).
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.is_strict()`

### `script_mods_list()` → Array[Dictionary] {name, path}

- **Since** 1.1 · no capability needed (API metadata)
- **Does / side effects:** Script mods loaded from `mods/api_mods/<name>/mod.gd`.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.script_mods_list()`

### `load_script_mods()` → void

- **Since** 1.1 · no capability needed (API metadata)
- **Does / side effects:** Internal: called once (deferred) by the API's `_ready` to load script mods when the version pin matched. Calling it again loads nothing new unless new folders appeared (they get new nodes).
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `# called by the core`

## Config and state

### `load_config(mod: String, category: String)` → Dictionary

- **Since** 1.0 · **capability** `config.files` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Merged `mods/<category>/*.json` (sorted by file name, later wins). Read-only.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.load_config("my_mod", "goofy")`

### `state_get(mod: String, key: String, default: Variant)` → Variant

- **Since** 1.1 · **capability** `config.state` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Value from this mod's state file `mods/api/state/<mod>.json`, or `default`.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.state_get("my_mod", "hits", 0)`

### `state_set(mod: String, key: String, value: Variant)` → bool

- **Since** 1.1 · **capability** `config.state` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Writes the key (JSON types) with an atomic tmp+rename write. Creates the folder.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.state_set("my_mod", "hits", 12)`

### `state_erase(mod: String, key: String)` → bool

- **Since** 1.1 · **capability** `config.state` · **access** `local_ui` · **scope** this PC only · _undo: allowed while the class is closed_
- **Does / side effects:** Removes the key; false when absent.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies. Undo variant works in every mode.
- **Example:** `api.state_erase("my_mod", "hits")`

### `state_keys(mod: String)` → PackedStringArray

- **Since** 1.1 · **capability** `config.state` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Keys in this mod's state file.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.state_keys("my_mod")`

### `config_validate(mod: String, library: String, config: Dictionary)` → Dictionary {ok, errors}

- **Since** 1.1 · **capability** `config.validate` · **access** `read` · **scope** this PC only
- **Does / side effects:** Validates a config dictionary with a library's own validator (`shots`, `goofy`). No side effects.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.config_validate("my_mod", "shots", cfg)`

### `libraries_status(mod: String)` → Dictionary {lib: {present, has_status}, gate_libraries}

- **Since** 1.1 · **capability** `lib.status` · **access** `read` · **scope** this PC only
- **Does / side effects:** Which subsystems are installed: loader, textures, models, shots, goofy, menu_ui, menu_perf, picker, plus the gate's per-library load state.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.libraries_status("my_mod").goofy.present`

### `library_status(mod: String, library: String)` → Dictionary

- **Since** 1.1 · **capability** `lib.status` · **access** `read` · **scope** this PC only
- **Does / side effects:** That library's own status dictionary (`get_status` / `get_info`). Empty when absent.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.library_status("my_mod", "shots")`

### `library_reload(mod: String, library: String)` → bool

- **Since** 1.1 · **capability** `lib.reload` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Re-reads the library's config files (same as the Mods panel reload); the library re-applies under its own gate.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.library_reload("my_mod", "goofy")`

## Events and signals

Events are delivered both as the API signal `event(name, data)` and to `subscribe` callables `fn(name: String, data: Dictionary)`. Subscribing needs `events.bus` plus the event's own capability. State-change events are polled at 4 Hz from cheap getters (no tree walks); goofy events are relayed from Goofy API 1.0 signals the moment they fire.

### `events_list()` → Array[Dictionary] {name, capability, data, when}

- **Since** 1.1 · no capability needed (API metadata)
- **Does / side effects:** Every event the bus can deliver (table below).
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.events_list()`

### `subscribe(mod: String, event: String, callable: Callable)` → bool

- **Since** 1.1 · **capability** `events.bus` · **access** `read` · **scope** this PC only
- **Does / side effects:** Adds a subscriber (idempotent). Freed callables are dropped automatically.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.subscribe("my_mod", "mode_changed", func(n, d): print(d.mode))`

### `unsubscribe(mod: String, event: String, callable: Callable)` → bool

- **Since** 1.1 · no capability needed (API metadata)
- **Does / side effects:** Removes the subscriber; false if it was not subscribed.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.unsubscribe("my_mod", "mode_changed", cb)`

### `connect_ball_event(mod: String, ball: Node, name: String, callable: Callable)` → bool

- **Since** 1.1 · **capability** `ball.events` · **access** `read` · **scope** this PC only · _`struck` uses ball.struck_
- **Does / side effects:** Connects to a Ball signal by name: bounced, struck, net_hit, wall_hit, antenna_hit (ball.gd:13-19). Anything else (incl. rpc) refused.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.connect_ball_event("my_mod", b, "struck", _on_struck)`

### `connect_player_event(mod: String, player: Node, name: String, callable: Callable)` → bool

- **Since** 1.1 · **capability** `player.events` · **access** `read` · **scope** this PC only · _`touched` uses player.touched_
- **Does / side effects:** Connects to a Player signal: touched, swung, thumped, knock_started, touch_attempted, out_of_range, launched, jumped, callout_made, high_fived, block_broken, dove_into_net, dove_into_net_pole (player.gd:126-162).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.connect_player_event("my_mod", p, "jumped", _on_jump)`

### `connect_net_signal(mod: String, name: String, callable: Callable)` → bool

- **Since** 1.1 · **capability** `net.game_signals` · **access** `read` · **scope** this PC only
- **Does / side effects:** Observes a session/lobby/party signal of the game's Net autoload: lobby_changed, lobby_tod_changed, session_ended, match_started, peer_left_match, peer_match_ready, late_joiner, party_joined, party_became_room, party_ball, party_stage, party_look. Input/snapshot streams, chat and every @rpc are refused.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.connect_net_signal("my_mod", "match_started", _on_match)`

### `connect_ball_struck(mod: String, ball: Node, callable: Callable, flags: int)` → bool

- **Since** 1.0 · **capability** `ball.struck` · **access** `read` · **scope** this PC only
- **Does / side effects:** 1.0 form of `connect_ball_event(..., "struck")` with connect flags.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.connect_ball_struck("my_mod", b, cb, 0)`

### `connect_player_touched(mod: String, player: Node, callable: Callable)` → bool

- **Since** 1.0 · **capability** `player.touched` · **access** `read` · **scope** this PC only
- **Does / side effects:** 1.0 form for `touched(kind)` (after the rally judged it).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.connect_player_touched("my_mod", p, cb)`

## Scene, players, rally (game internals by meaning)

### `get_current_scene(mod: String)` → Node

- **Since** 1.0 · **capability** `scene.current` · **access** `read` · **scope** this PC only
- **Does / side effects:** Running scene (menu, match, practice court).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.get_current_scene("my_mod")`

### `find_balls(mod: String)` → Array[Node]

- **Since** 1.0 · **capability** `scene.balls` · **access** `read` · **scope** this PC only
- **Does / side effects:** Every node whose script is class Ball.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.find_balls("my_mod")`

### `find_players(mod: String)` → Array[Node]

- **Since** 1.0 · **capability** `scene.players` · **access** `read` · **scope** this PC only
- **Does / side effects:** Every Player.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.find_players("my_mod")`

### `is_ball(mod: String, o: Object)` → bool

- **Since** 1.0 · **capability** `scene.balls` · **access** `read` · **scope** this PC only
- **Does / side effects:** Is this object a Ball.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.is_ball("my_mod", body)`

### `get_local_player(mod: String)` → Node

- **Since** 1.0 · **capability** `scene.players` · **access** `read` · **scope** this PC only
- **Does / side effects:** The local player (`player` on the current scene).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.get_local_player("my_mod")`

### `get_rally_for(mod: String, player: Node)` → Object

- **Since** 1.0 · **capability** `scene.rally` · **access** `read` · **scope** this PC only
- **Does / side effects:** The Rally judging this player (main.gd `_rally`).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.get_rally_for("my_mod", p)`

### `stage_get_nodes(mod: String)` → Dictionary {stage, court, court_floor, net, grass}

- **Since** 1.1 · **capability** `scene.stage` · **access** `read` · **scope** this PC only
- **Does / side effects:** Stage/court nodes of the current scene (null entries when missing). Read-only handles.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.stage_get_nodes("my_mod").court`

### `player_get_state(mod: String, player: Node)` → Dictionary {team_side, serving, contact_state, aim_point, hold_power, grounded, position}

- **Since** 1.1 · **capability** `player.input_state` · **access** `read` · **scope** this PC only
- **Does / side effects:** What the player is doing (player.gd fields). Read-only.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.player_get_state("my_mod", p).serving`

### `player_feel_get(mod: String, prop: String)` → Variant

- **Since** 1.1 · **capability** `player.feel` · **access** `read` · **scope** this PC only
- **Does / side effects:** A PlayerFeel field (player_feel.tres). Writes go through `goofy_set_override`.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.player_feel_get("my_mod", "hit_range")`

### `get_net_state(mod: String)` → Dictionary {found, active, is_host, lobby_id, steam, party_lan, custom_lobby}

- **Since** 1.0 · **capability** `net.state` · **access** `read` · **scope** this PC only
- **Does / side effects:** Online session state from the Net autoload.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.get_net_state("my_mod").is_host`

## Lobby data (sync)

### `lobby_data_get(mod: String, key: String)` → String

- **Since** 1.0 · **capability** `net.lobby_data` · **access** `sync` · **scope** sent to modded peers
- **Does / side effects:** Steam lobby data value; only `jvm_*` keys.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.lobby_data_get("my_mod", "jvm_rules")`

### `lobby_data_set(mod: String, key: String, value: String)` → bool

- **Since** 1.0 · **capability** `net.lobby_data` · **access** `sync` · **scope** sent to modded peers
- **Does / side effects:** Host only: writes a `jvm_*` lobby key every member can read.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.lobby_data_set("my_mod", "jvm_rules", "3|1")`

### `lobby_data_clear(mod: String, key: String)` → bool

- **Since** 1.0 · **capability** `net.lobby_data` · **access** `sync` · **scope** sent to modded peers · _undo: allowed while closed_
- **Does / side effects:** Host only: clears the key.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies. Undo variant works in every mode.
- **Example:** `api.lobby_data_clear("my_mod", "jvm_rules")`

## Ball

### `ball_physics_open(mod: String, path: String)` → String (error or "")

- **Since** 1.0 · **capability** `ball.physics` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Opens the shared BallPhysics resource (`""` = the game's PHYS) for this mod.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.ball_physics_open("my_mod", "")`

### `ball_physics_close(mod: String)` → void

- **Since** 1.0 · **capability** `ball.physics` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Forgets this mod's handle (does not restore values).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.ball_physics_close("my_mod")`

### `ball_physics_get(mod: String, prop: String)` → Variant

- **Since** 1.0 · **capability** `ball.physics` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Reads a BallPhysics field.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.ball_physics_get("my_mod", "radius")`

### `ball_physics_set(mod: String, prop: String, value: Variant)` → bool

- **Since** 1.0 · **capability** `ball.physics` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Writes a BallPhysics field (unknown fields refused). Affects every ball.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.ball_physics_set("my_mod", "radius", 0.4)`

### `ball_physics_restore(mod: String, prop: String, value: Variant)` → bool

- **Since** 1.0 · **capability** `ball.physics` · **access** `gameplay` · **scope** this PC only · _undo: allowed while closed_
- **Does / side effects:** Writes back a saved value.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown. Undo variant works in every mode.
- **Example:** `api.ball_physics_restore("my_mod", "radius", r0)`

### `ball_get_model(mod: String, ball: Node)` → Node

- **Since** 1.0 · **capability** `ball.model` · **access** `read` · **scope** this PC only
- **Does / side effects:** The ball's dressed visual model.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.ball_get_model("my_mod", b)`

### `ball_get_flight(mod: String, ball: Node)` → Dictionary {vel, curve, dive_g, strike_seq, hit_by}

- **Since** 1.0 · **capability** `ball.flight` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Flight state.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.ball_get_flight("my_mod", b).vel`

### `ball_set_flight(mod: String, ball: Node, flight: Dictionary)` → bool

- **Since** 1.0 · **capability** `ball.flight` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Writes vel / curve / dive_g.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.ball_set_flight("my_mod", b, {"curve": 0.5})`

### `node_get_scale(mod: String, cap: String, node: Node)` → Vector3

- **Since** 1.0 · **capability** `ball.scale` / `player.scale` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Scale of a ball (`ball.scale`) or player (`player.scale`).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.node_get_scale("my_mod", "ball.scale", b)`

### `node_set_scale(mod: String, cap: String, node: Node, scale: Vector3)` → bool

- **Since** 1.0 · **capability** `ball.scale` / `player.scale` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Sets it.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.node_set_scale("my_mod", "player.scale", p, Vector3.ONE * 2)`

### `node_restore_scale(mod: String, cap: String, node: Node, scale: Vector3)` → bool

- **Since** 1.0 · **capability** `ball.scale` / `player.scale` · **access** `gameplay` · **scope** this PC only · _undo: allowed while closed_
- **Does / side effects:** Restores a saved scale.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown. Undo variant works in every mode.
- **Example:** `api.node_restore_scale("my_mod", "player.scale", p, s0)`

## Rules and the Modifiers panel

### `rally_touch_state(mod: String, rally: Object)` → Dictionary {touch_count, touch_side, last_toucher, practice}

- **Since** 1.0 · **capability** `rules.touch_count` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Rally touch bookkeeping.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.rally_touch_state("my_mod", r)`

### `rally_set_touch_count(mod: String, rally: Object, count: int)` → bool

- **Since** 1.0 · **capability** `rules.touch_count` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Rewrites the count (host-enforced).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.rally_set_touch_count("my_mod", r, 0)`

### `rally_clear_last_toucher(mod: String, rally: Object)` → bool

- **Since** 1.0 · **capability** `rules.double_hit` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Allows the same player to touch again.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.rally_clear_last_toucher("my_mod", r)`

### `modifiers_panel_find(mod: String)` → Node

- **Since** 1.0 · **capability** `rules.modifiers_panel` · **access** `gameplay` · **scope** this PC only · _allowed while closed (to hide rows)_
- **Does / side effects:** The custom-lobby Modifiers panel. Uses the menu's own `_modifiers` pointer, then the cached panel; tree walk only as fallback (menu perf).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.modifiers_panel_find("my_mod")`

### `modifiers_panel_rows(mod: String, panel: Node)` → Array or null

- **Since** 1.0 · **capability** `rules.modifiers_panel` · **access** `gameplay` · **scope** this PC only · _allowed while closed_
- **Does / side effects:** This mod's rows (null = never added).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.modifiers_panel_rows("my_mod", panel)`

### `modifiers_panel_add_rows(mod: String, panel: Node, defs: Array, on_changed: Callable)` → Dictionary

- **Since** 1.0 · **capability** `rules.modifiers_panel` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Adds rows built with the panel's own `_rule` / `_entry_row`.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.modifiers_panel_add_rows("my_mod", panel, defs, _on_changed)`

### `modifiers_panel_show_rows(mod: String, rows: Array, show: bool)` → void

- **Since** 1.0 · **capability** `rules.modifiers_panel` · **access** `gameplay` · **scope** this PC only · _allowed while closed_
- **Does / side effects:** Shows/hides this mod's rows.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.modifiers_panel_show_rows("my_mod", rows, false)`

### `modifiers_panel_set_values(mod: String, panel: Node, values: Dictionary)` → void

- **Since** 1.0 · **capability** `rules.modifiers_panel` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Sets row values quietly.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.modifiers_panel_set_values("my_mod", panel, {"max_hits": 3})`

### `goofy_rules_get(mod: String)` → Dictionary {max_hits, no_double_hit}

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Goofy's rules.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_rules_get("my_mod")`

### `goofy_rules_set(mod: String, max_hits: int, no_double_hit: bool)` → bool

- **Since** 1.1 · **capability** `goofy.rules` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Sets Goofy's rules (host enforces; shown in the Modifiers panel; lobby key `jvm_rules` syncs it in LAN/private/friends).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.goofy_rules_set("my_mod", 4, true)`

## Engine, overrides, generic escape hatch

### `time_scale_get()` → float

- **Since** 1.0 · no capability needed (API metadata)
- **Does / side effects:** Engine.time_scale.
- **Multiplayer:** Safe everywhere (no game state touched).
- **Example:** `api.time_scale_get()`

### `time_scale_set(mod: String, value: float)` → bool

- **Since** 1.0 · **capability** `engine.time_scale` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Sets Engine.time_scale on this PC (in a synced match Goofy applies the host's value; see Decisions).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.time_scale_set("my_mod", 0.5)`

### `time_scale_restore(mod: String, value: float)` → bool

- **Since** 1.0 · **capability** `engine.time_scale` · **access** `gameplay` · **scope** this PC only · _undo: allowed while closed_
- **Does / side effects:** Restores it.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown. Undo variant works in every mode.
- **Example:** `api.time_scale_restore("my_mod", 1.0)`

### `overrides_set_rules(mod: String, rules: Array)` → void

- **Since** 1.0 · **capability** `engine.overrides` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** This mod's override rules ({target, set}).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.overrides_set_rules("my_mod", rules)`

### `overrides_apply(mod: String)` → int (nodes changed)

- **Since** 1.0 · **capability** `engine.overrides` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Applies them (reversible, per mod).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.overrides_apply("my_mod")`

### `overrides_revert(mod: String)` → void

- **Since** 1.0 · **capability** `engine.overrides` · **access** `gameplay` · **scope** this PC only · _undo: allowed while closed_
- **Does / side effects:** Reverts every override of this mod.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown. Undo variant works in every mode.
- **Example:** `api.overrides_revert("my_mod")`

### `game_get(mod: String, object: Object, prop: String)` → Variant

- **Since** 1.0 · **capability** `game.generic` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Escape hatch (logged): read a property.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.game_get("my_mod", n, "x")`

### `game_set(mod: String, object: Object, prop: String, value: Variant)` → bool

- **Since** 1.0 · **capability** `game.generic` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Escape hatch: write a property.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.game_set("my_mod", n, "x", 5)`

### `game_call(mod: String, object: Object, method: String, args: Array)` → Variant

- **Since** 1.0 · **capability** `game.generic` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Escape hatch: call a method. @rpc methods, rpc, rpc_id, rpc_config, set_multiplayer_authority refused.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.game_call("my_mod", n, "ok", [])`

### `game_connect(mod: String, object: Object, signal: String, callable: Callable)` → bool

- **Since** 1.0 · **capability** `game.generic` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Escape hatch: connect a signal.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.game_connect("my_mod", n, "changed", cb)`

## Packs, textures, stage / ball picks

Translation to the unified loader's texture subsystem.

### `content_status(mod: String)` → Dictionary {sharing, repo, auto_download, offer, peers, catalog, downloads, downloaded, rejected, offers_sent, offers_received, shown_ball, shown_stage, cached}

- **Since** 1.2 · **capability** `net.content_offer` · **access** `sync` · **scope** sent to modded peers
- **Does / side effects:** Cosmetic sharing state: what this player advertises, what every modded peer wears (per item: have / downloading / installed / hash mismatch / ...), the assets-repo catalog and downloads (docs/CONTENT-SHARING.md). Readable in any mode; sharing itself runs only in LAN / private / friends.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.content_status("my_mod").peers`

### `content_fetch(mod: String, id: String)` → bool

- **Since** 1.2 · **capability** `net.content_download` · **access** `sync` · **scope** sent to modded peers
- **Does / side effects:** Queue one assets-repo item now (retry after a failed download). Same checks as the automatic path; only in a synced session with the manifest read.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.content_fetch("my_mod", "1011")`

### `packs_list(mod: String, type: String)` → Array[Dictionary]

- **Since** 1.1 · **capability** `packs.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Picker packs of a type (`stage`, `ball`).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.packs_list("my_mod", "stage")`

### `pack_info(mod: String, id: String)` → Dictionary

- **Since** 1.1 · **capability** `packs.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** One pack's parsed skin.json view.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.pack_info("my_mod", "1008")`

### `packs_rescan(mod: String)` → bool

- **Since** 1.1 · **capability** `packs.rescan` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Re-reads pack folders (cached images reused), re-applies picks.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.packs_rescan("my_mod")`

### `stage_get_picked(mod: String)` → String

- **Since** 1.1 · **capability** `packs.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Picked custom court id ("" = stock).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.stage_get_picked("my_mod")`

### `stage_set_picked(mod: String, id: String)` → bool

- **Since** 1.1 · **capability** `stage.pick` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Picks a custom court (saved like the picker does). `""` = stock.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.stage_set_picked("my_mod", "1008")`

### `ball_get_picked(mod: String)` → String

- **Since** 1.1 · **capability** `packs.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Picked custom ball id.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.ball_get_picked("my_mod")`

### `ball_set_picked(mod: String, id: String)` → bool

- **Since** 1.1 · **capability** `ball.pick` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Picks a custom ball.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.ball_set_picked("my_mod", "1009")`

### `cosmetics_apply(mod: String)` → bool

- **Since** 1.1 · **capability** `stage.pick` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Applies picked court/ball cosmetics now.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.cosmetics_apply("my_mod")`

### `textures_status(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `packs.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Texture subsystem status.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.textures_status("my_mod")`

### `textures_sync_status(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `packs.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Texture sync (modded peers) status.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.textures_sync_status("my_mod")`

### `textures_cache_info(mod: String)` → Dictionary {count, disk_loads, ids, swapped, overrides, spawned, mips}

- **Since** 1.1 · **capability** `packs.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Process-lifetime texture cache (pack-persist / perf) and mipmap stats (ground moire fix).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.textures_cache_info("my_mod").count`

### `texture_get_cached(mod: String, path: String)` → Texture2D or null

- **Since** 1.1 · **capability** `packs.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** A cached pack texture by full path.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.texture_get_cached("my_mod", p)`

## Courts: environment and sky

### `court_env_info(mod: String, stage_id: String)` → Dictionary {found, has_environment, has_sky, environment, sky, court, ...}

- **Since** 1.1 · **capability** `court.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** A stage pack's environment{} / sky{} / court{} blocks.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.court_env_info("my_mod", "10E1")`

### `court_env_status(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `court.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Environment GLB injection status (clearance verdicts, spawned).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.court_env_status("my_mod")`

### `court_sky_status(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `court.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Distant sky status.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.court_sky_status("my_mod")`

### `court_env_load(mod: String, stage_id: String)` → bool

- **Since** 1.1 · **capability** `court.env` · **access** `cosmetic` · **scope** this PC only · _also needs stage.pick_
- **Does / side effects:** Picks the court and applies it now: floor/net look + environment GLB (clearance-checked) + sky.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.court_env_load("my_mod", "10E1")`

## Native audio (1.4)

Lazy access to the game's own sound effects and ambience tracks through the unified loader. Enumeration is a cheap index of `res://` paths (`res://game/sfx`, `res://game/ost`); a stream is loaded and a player/host node is created only on the first play request, then freed when unused. Playback is routed through the game's existing Master / sfx / music / ambient / crowd / announcer buses so the user's volume settings apply. Stage packs may declare `audio: { ambience, bus, volume }` in `skin.json`; no shipped pack uses it yet.

### `audio_list(mod: String)` → Array[Dictionary] {id, path, category, bus}

- **Since** 1.4 · **capability** `audio.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Native sounds as a path index only. Does not load streams or create players/nodes.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.audio_list("my_mod").size()`

### `audio_buses(mod: String)` → PackedStringArray

- **Since** 1.4 · **capability** `audio.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Live AudioServer bus names (Master, sfx, music, ambient, ...). Cheap; does not start playback.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.audio_buses("my_mod")`

### `audio_info(mod: String, id: String)` → Dictionary {found, id, path, category, bus}

- **Since** 1.4 · **capability** `audio.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Resolve a native sound by short id, alias (`beach`) or `res://` path. Builds the path index if needed; does not load the stream.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.audio_info("my_mod", "beach")`

### `audio_status(mod: String)` → Dictionary {started, index_built, index_count, cached_streams, players, nodes, idle, ...}

- **Since** 1.4 · **capability** `audio.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Idle means started=false and no players, host node or cached streams. Listing sounds does not leave idle.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.audio_status("my_mod").idle`

### `audio_play(mod: String, id: String, opts: Dictionary = {})` → int (handle, 0 = fail)

- **Since** 1.4 · **capability** `audio.play` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Play a native one-shot. opts: volume (linear), bus, position (Vector3, optional 3D). Creates the host node and loads the stream on first use.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.audio_play("my_mod", "announcer/ace")`

### `audio_play_loop(mod: String, id: String, opts: Dictionary = {})` → int (handle, 0 = fail)

- **Since** 1.4 · **capability** `audio.play` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Play a native sound looped (ambience). Same opts as audio_play. The beach ambience id is `beach` / `ambiance/717146__ghosttropic__short-sand-beach-waves-3`.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.audio_play_loop("my_mod", "beach", {"bus": "ambient"})`

### `audio_stop(mod: String, handle_or_id: Variant)` → bool

- **Since** 1.4 · **capability** `audio.play` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Stop a handle (int) or every voice of a sound id (String). Frees the stream when unused; frees the host node when nothing is playing.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.audio_stop("my_mod", h)`

### `audio_stop_all(mod: String)` → bool

- **Since** 1.4 · **capability** `audio.play` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Stop every voice started through this API and return to the idle (zero-cost) state.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.audio_stop_all("my_mod")`

### `audio_set_volume(mod: String, handle: int, volume: float)` → bool

- **Since** 1.4 · **capability** `audio.play` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Per-voice linear gain (the bus still follows the game's Master/sfx/music/ambient volume).
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.audio_set_volume("my_mod", h, 0.5)`

### `audio_set_bus(mod: String, handle: int, bus: String)` → bool

- **Since** 1.4 · **capability** `audio.play` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Move a voice to a game bus (ambient, sfx, music, crowd, announcer, Master, feedback). Unknown names fall back.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.audio_set_bus("my_mod", h, "ambient")`

### `audio_apply_pack(mod: String, stage_id: String)` → int (handle, 0 = none)

- **Since** 1.4 · **capability** `audio.play` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Apply a stage pack's audio{} block (looped ambience). No-op when the pack has no audio{}.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.audio_apply_pack("my_mod", "10F8")`

## Accessories and player models

### `accessories_status(mod: String)` → Dictionary {injected, catalog_added, catalog_total, entries, worn, unplaced}

- **Since** 1.1 · **capability** `accessories.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Locker accessories from packs (Head / Face / Extra).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.accessories_status("my_mod")`

### `accessories_list(mod: String, category: String)` → Array[Dictionary] {id, category, part, pack}

- **Since** 1.1 · **capability** `accessories.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Entries of one category (`head`, `face`, `extra`; "" = all).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.accessories_list("my_mod", "head")`

### `models_list(mod: String)` → Array

- **Since** 1.1 · **capability** `models.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Custom player models.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.models_list("my_mod")`

### `model_get_saved(mod: String)` → String

- **Since** 1.1 · **capability** `models.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Saved model id.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.model_get_saved("my_mod")`

### `model_get_shown(mod: String)` → String

- **Since** 1.1 · **capability** `models.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Model shown now (preview or saved).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.model_get_shown("my_mod")`

### `model_is_previewing(mod: String)` → bool

- **Since** 1.1 · **capability** `models.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Locker preview active.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.model_is_previewing("my_mod")`

### `model_settings_get(mod: String, id: String)` → Dictionary

- **Since** 1.1 · **capability** `models.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Per-model settings (colour, options).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.model_settings_get("my_mod", id)`

### `models_status(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `models.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Model subsystem status.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.models_status("my_mod")`

### `models_rig_report(mod: String)` → Variant

- **Since** 1.1 · **capability** `models.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Rig graft report (debug).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.models_rig_report("my_mod")`

### `model_preview(mod: String, id: String)` → bool

- **Since** 1.1 · **capability** `player.model` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Previews a model (not saved).
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.model_preview("my_mod", id)`

### `model_apply(mod: String, id: String)` → bool

- **Since** 1.1 · **capability** `player.model` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Applies + saves a model.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.model_apply("my_mod", id)`

### `model_end_preview(mod: String)` → bool

- **Since** 1.1 · **capability** `player.model` · **access** `cosmetic` · **scope** this PC only · _undo: allowed while closed_
- **Does / side effects:** Ends a preview.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context. Undo variant works in every mode.
- **Example:** `api.model_end_preview("my_mod")`

### `model_set_color(mod: String, id: String, color: Color)` → bool

- **Since** 1.1 · **capability** `player.model` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Sets a model's tint (saved).
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.model_set_color("my_mod", id, Color.RED)`

### `model_reset_color(mod: String, id: String)` → bool

- **Since** 1.1 · **capability** `player.model` · **access** `cosmetic` · **scope** this PC only · _undo: allowed while closed_
- **Does / side effects:** Resets the tint.
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context. Undo variant works in every mode.
- **Example:** `api.model_reset_color("my_mod", id)`

### `model_set_option(mod: String, id: String, key: String, value: Variant)` → bool

- **Since** 1.1 · **capability** `player.model` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Sets a model option (hands, target...).
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.model_set_option("my_mod", id, "hands", 1)`

## Unified loader

### `loader_status(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `loader.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Unified loader status.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.loader_status("my_mod")`

### `loader_bridge_status(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `loader.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Goofy bridge state (map, applied pack, goofy version).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.loader_bridge_status("my_mod")`

### `loader_is_map(mod: String, stage_id: String)` → bool

- **Since** 1.1 · **capability** `loader.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Is this stage a loader map.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.loader_is_map("my_mod", "1022")`

### `loader_active_map(mod: String)` → String

- **Since** 1.1 · **capability** `loader.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Current loader map ("" = stock).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.loader_active_map("my_mod")`

### `loader_map_goofy_block(mod: String, stage_id: String)` → Dictionary

- **Since** 1.1 · **capability** `loader.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** A map's goofy{} block.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.loader_map_goofy_block("my_mod", "1022")`

### `loader_refresh(mod: String)` → bool

- **Since** 1.1 · **capability** `stage.pick` · **access** `cosmetic` · **scope** this PC only
- **Does / side effects:** Re-evaluates the current map (re-sends goofy block).
- **Multiplayer:** Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.
- **Example:** `api.loader_refresh("my_mod")`

## Goofy ball (all 156 params)

Translation to the Goofy API (docs/GOOFY-API.md). The 156-param ball-library schema (BallPhysics x124, PlayerFeel x27, Goofy flight extras x5) is also compiled into core (src/gate/goofy_params.inc), so discovery works without Goofy. Mod layers have owner `mod:<name>`; precedence stock < user config (100) < court pack (200) < mods (default 500, 0..899) < FIXED.

### `goofy_available(mod: String)` → bool

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Goofy API 1.0 installed.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_available("my_mod")`

### `goofy_api_version(mod: String)` → String

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** "1.2" ("1.0" for older Goofy builds) or "".
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_api_version("my_mod")`

### `goofy_bridge_info(mod: String)` → Dictionary {id, version, enabled} ({} = absent)

- **Since** 1.1 · **capability** `goofy.packs` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** The goofy library as the loader bridge sees it: instance id (changes when it is replaced), its API version and whether it is enabled. Declaration-checked only.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.goofy_bridge_info("my_mod").get("version", "")`

### `goofy_loader_map_set(mod: String, map: String)` → bool

- **Since** 1.1 · **capability** `goofy.packs` · **access** `gameplay` · **scope** this PC only · _Goofy applies map layers live only where its physics tier allows (offline / LAN / private)._
- **Does / side effects:** Tells goofy which loader map is in play ("" = none). Used by the unified loader.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.goofy_loader_map_set("my_mod", "beach_day")`

### `goofy_pack_apply(mod: String, map: String, params: Dictionary, version: int)` → Dictionary {ok, applied, ignored, errors, active}

- **Since** 1.1 · **capability** `goofy.packs` · **access** `gameplay` · **scope** this PC only · _As above._
- **Does / side effects:** Hands a map's `goofy{}` block to goofy's pack layer (Goofy API `apply_pack_params`).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.goofy_pack_apply("my_mod", "beach_day", {"ball.gravity": {"mul": 0.9}}, 1)`

### `goofy_pack_clear(mod: String, map: String)` → bool

- **Since** 1.1 · **capability** `goofy.packs` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Removes that map's pack layer (the map itself stays).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.goofy_pack_clear("my_mod", "beach_day")`

### `goofy_schema(mod: String)` → Array[Dictionary] {id, section, target, prop, default, min, max, unit, apply, sync}

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Every param of both libraries (from Goofy when installed, else the core table); 1.2 entries also carry `library` (ball / player), `type` (float / int / color) and `is_color`.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_schema("my_mod").size()  # 351 = 156 ball + 195 player`

### `goofy_param_info(mod: String, id: String)` → Dictionary

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** One param from the core table.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_param_info("my_mod", "ball.gravity")`

### `goofy_param_get(mod: String, id: String)` → Variant

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Effective value now (Goofy), or the live resource value without Goofy.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_param_get("my_mod", "ball.gravity")`

### `goofy_params(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Params differing from stock.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_params("my_mod")`

### `goofy_stock(mod: String, id: String)` → Variant

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** The game's stock value.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_stock("my_mod", "ball.gravity")`

### `goofy_layers(mod: String)` → Array[Dictionary]

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Layers in precedence order (debug).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_layers("my_mod")`

### `goofy_ball_mode_get(mod: String)` → String

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** "normal" / "fixed".
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_ball_mode_get("my_mod")`

### `goofy_loader_map(mod: String)` → String

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Goofy's view of the loader map.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_loader_map("my_mod")`

### `goofy_physics_allowed(mod: String)` → bool

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Goofy values live now: offline modes (local layers) or LAN / private / friends lobbies (own values in the lobby, the host's gameplay values during a match, see `goofy_sync_status`). Never in public lobbies.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_physics_allowed("my_mod")`

### `goofy_status(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Goofy library status.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_status("my_mod")`

### `goofy_set_override(mod: String, params: Dictionary, priority: int = 500, scope: String = "session")` → Dictionary {ok, applied, ignored, errors, active}

- **Since** 1.1 · **capability** `goofy.ball` · **access** `offline` · **scope** this PC only
- **Does / side effects:** Replaces this mod's BALL layer (ball-library ids; player-library ids are refused into `errors`, use `goofy_player_set_override`). Values: number, [r,g,b,a] or {"mul": f}. Priority clamped to 0..899, scope `session` or `stage`. Kept while closed only inside Goofy; the API refuses new writes outside offline modes.
- **Multiplayer:** Unsynced engine-wide change: open ONLY in Free Play / practice / offline solo / menu; refused in every online lobby (LAN/private/friends too) so nothing desyncs.
- **Example:** `api.goofy_set_override("my_mod", {"ball.gravity": {"mul": 0.5}})`

### `goofy_clear_override(mod: String)` → bool

- **Since** 1.1 · **capability** `goofy.ball` · **access** `offline` · **scope** this PC only · _undo: allowed while closed_
- **Does / side effects:** Removes this mod's layer.
- **Multiplayer:** Unsynced engine-wide change: open ONLY in Free Play / practice / offline solo / menu; refused in every online lobby (LAN/private/friends too) so nothing desyncs. Undo variant works in every mode.
- **Example:** `api.goofy_clear_override("my_mod")`

### `goofy_ball_mode_set(mod: String, mode: String)` → bool

- **Since** 1.1 · **capability** `goofy.ball_mode` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Ball override NORMAL/FIXED (persisted by Goofy).
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.goofy_ball_mode_set("my_mod", "fixed")`

## Player consts (1.2)

The 34 gameplay values hard-coded in game/player/player.gd (docs/PLAYER-CONSTS.md). **HARD REQUIREMENT: every player needs player.consts (same game build). If anyone in the match lacks it, the host turns the edited values off for the whole match and everyone plays stock; the host gets a notice naming who.** Values are match-wide. Edit in the lobby / menu only: at match start the set is frozen and the host sends it to every player; any write during a match (API, Goofy UI, sync packet) is refused with a logged warning and an `error`. Unlocked again at match end (exact stock restored).

### `player_consts_get(mod: String)` → Dictionary {available, reason, manifest_id, locked, applied, saved, in_play, stock, notice, rejected, warnings}

- **Since** 1.1 · **capability** `player.consts` · **access** `gameplay` · **scope** this PC only · _read: allowed everywhere_
- **Does / side effects:** State: whether the boot rewrite is active, the saved lobby set, the values live now, the last host notice.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.player_consts_get("my_mod")`

### `player_consts_set(mod: String, name: String, value: float)` → Dictionary {ok, error}

- **Since** 1.1 · **capability** `player.consts` · **access** `gameplay` · **scope** this PC only · _lobby only_
- **Does / side effects:** Saves one value (clamped to its range) for the next match. During a match: `ok=false` + error, value unchanged.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.player_consts_set("my_mod", "BLOCK_TURN", 0.5)`

### `player_consts_reset(mod: String, name: String)` → Dictionary {ok, error}

- **Since** 1.1 · **capability** `player.consts` · **access** `gameplay` · **scope** this PC only · _lobby only_
- **Does / side effects:** Back to the stock value (`"*"` = all 34). Refused during a match like `player_consts_set`.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.player_consts_reset("my_mod", "*")`

### `player_consts_locked(mod: String)` → bool

- **Since** 1.1 · **capability** `player.consts` · **access** `gameplay` · **scope** this PC only · _read_
- **Does / side effects:** True while a match runs (edits refused, UI greyed).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.player_consts_locked("my_mod")`

## Goofy ball + player libraries, host sync (1.2)

Goofy API 1.2 (docs/GOOFY-BALL.md, docs/GOOFY-PLAYER.md, docs/GOOFY-SYNC.md). Ball and player indices are positions in `goofy_balls()` / `goofy_players()` (= `find_balls` / `find_players` order) at the time of the call. Live state reads are read-only and safe everywhere; writes need an offline mode. In LAN / private / friends lobbies the HOST's effective params are used by every modded player (clients ignore their own layers until they leave); public lobbies never sync and never apply Goofy physics.

### `goofy_ball_schema(mod: String)` → Array[Dictionary]

- **Since** 1.2 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** The 156 ball-library params: physics, bounce / collision, hitbox, net geometry, serve, shot shaping, speed limits, spin / curve, visuals (outline / trail / contest), hit range, player physics, drag.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_ball_schema("my_mod").size()  # 156`

### `goofy_player_schema(mod: String)` → Array[Dictionary]

- **Since** 1.2 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** The 195 player-library params: movement, jump, sprint / stamina, dive / fall, block, serve, animation, visuals, contact timing (PlayerFeel) and `player.body_scale`.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_player_schema("my_mod").size()  # 195`

### `goofy_balls(mod: String)` → Array[Dictionary]

- **Since** 1.2 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** `goofy_ball_state` of every ball.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_balls("my_mod")`

### `goofy_ball_state(mod: String, index: int)` → Dictionary {index, position, velocity, speed, curve, dive_g, strike_seq, frozen, serve_held, net_puppet, remote_driven, float_serve, hitstop_left, hit_by, radius, scale, airborne}

- **Since** 1.2 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** One ball's live state; `hit_by` = player index or -1. {} for a bad index.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_ball_state("my_mod", 0).speed`

### `goofy_ball_set_velocity(mod: String, index: int, velocity: Vector3)` → bool

- **Since** 1.2 · **capability** `goofy.ball_live` · **access** `offline` · **scope** this PC only
- **Does / side effects:** Sets the ball's velocity now (finite only, length capped at 200). Unsynced, so offline only.
- **Multiplayer:** Unsynced engine-wide change: open ONLY in Free Play / practice / offline solo / menu; refused in every online lobby (LAN/private/friends too) so nothing desyncs.
- **Example:** `api.goofy_ball_set_velocity("my_mod", 0, Vector3(0, 8, -4))`

### `goofy_ball_set_curve(mod: String, index: int, curve: float)` → bool

- **Since** 1.2 · **capability** `goofy.ball_live` · **access** `offline` · **scope** this PC only
- **Does / side effects:** Sets the ball's curve (spin) now, clamped to -100..100. Offline only.
- **Multiplayer:** Unsynced engine-wide change: open ONLY in Free Play / practice / offline solo / menu; refused in every online lobby (LAN/private/friends too) so nothing desyncs.
- **Example:** `api.goofy_ball_set_curve("my_mod", 0, 2.5)`

### `goofy_ball_impulse(mod: String, index: int, delta_v: Vector3)` → bool

- **Since** 1.2 · **capability** `goofy.ball_live` · **access** `offline` · **scope** this PC only
- **Does / side effects:** Adds `delta_v` to the ball's velocity (capped at 200). Offline only.
- **Multiplayer:** Unsynced engine-wide change: open ONLY in Free Play / practice / offline solo / menu; refused in every online lobby (LAN/private/friends too) so nothing desyncs.
- **Example:** `api.goofy_ball_impulse("my_mod", 0, Vector3.UP * 3)`

### `goofy_players(mod: String)` → Array[Dictionary]

- **Since** 1.2 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** `goofy_player_state` of every player.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_players("my_mod")`

### `goofy_player_state(mod: String, index: int)` → Dictionary {index, local, team_side, serving, contact_state, aim_point, hold_power, grounded, position, velocity, speed, sprinting, stamina, is_bot, dive_cooldown, scale, body_scale, anim {clip, pos, speed}}

- **Since** 1.2 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** One player's live state incl. the animation state of its rig. {} for a bad index.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_player_state("my_mod", 0).anim`

### `goofy_player_cosmetics(mod: String, index: int)` → Dictionary {index, local, has_character, display_name, number, skin_tone, hair_style, hair_color, accessories {slot: id}, look, model}

- **Since** 1.2 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Read-only cosmetics state from the player's character config (+ the shown custom model for the local player).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_player_cosmetics("my_mod", 0).accessories`

### `goofy_player_set_override(mod: String, params: Dictionary, priority: int = 500, scope: String = "session")` → Dictionary {ok, applied, ignored, errors, active}

- **Since** 1.2 · **capability** `goofy.player` · **access** `offline` · **scope** this PC only
- **Does / side effects:** Replaces this mod's PLAYER layer (owner `mod:<name>:player`; player-library ids only, ball ids are refused into `errors`). Same values / priority / scope rules as `goofy_set_override`.
- **Multiplayer:** Unsynced engine-wide change: open ONLY in Free Play / practice / offline solo / menu; refused in every online lobby (LAN/private/friends too) so nothing desyncs.
- **Example:** `api.goofy_player_set_override("my_mod", {"player.move_speed": {"mul": 1.3}, "player.body_scale": 1.5})`

### `goofy_player_clear_override(mod: String)` → bool

- **Since** 1.2 · **capability** `goofy.player` · **access** `offline` · **scope** this PC only · _undo: allowed while closed_
- **Does / side effects:** Removes this mod's player layer.
- **Multiplayer:** Unsynced engine-wide change: open ONLY in Free Play / practice / offline solo / menu; refused in every online lobby (LAN/private/friends too) so nothing desyncs. Undo variant works in every mode.
- **Example:** `api.goofy_player_clear_override("my_mod")`

### `goofy_sync_status(mod: String)` → Dictionary {role, authoritative, live, host_left, host_values, time_scale, time_scale_synced, peers, sent, received, rejected, seq, last_error, version}

- **Since** 1.2 · **capability** `goofy.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Host sync: role off / offline / host / client, whose gameplay values are live (local / host / none; a client follows the host from match start to match end), the synced time scale and packet counters (payload version 2).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.goofy_sync_status("my_mod").role`

## Gameplay settings registry and game rules (1.3)

The translation layer for settings that change play (docs/GAMEPLAY-SETTINGS.md, docs/GOOFY-RULES.md). A mod registers its settings once during its config setup; the core persists them per mod (`mods/api/state/gameplay/<mod>.json`, versioned), builds their controls in the game's Settings panel (`mods` tab) and answers every read through `gameplay_get`: stock while gameplay is closed (public lobbies / quick match), the host's value for clients in LAN / private / friends lobbies (stock until the host's values arrive), else the local value. Values are always validated and clamped. Visual-only settings are refused: cosmetics stay per-player.

### `gameplay_register(mod: String, defs: Array[Dictionary], options: Dictionary {version, migrate, on_change})` → Dictionary {ok, registered, errors, version, migrated_from, loaded, role}

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while gameplay is closed (registering changes nothing in play)_
- **Does / side effects:** Registers settings. Each def: `id` (`<mod>.<name>`, lowercase), `type` (bool / int / float / enum / vec), `default`, `min` / `max` / `step` (numbers; vec per component), `options` (enum), `label`, `tooltip`, `category`, `order`, `sync` (must be `gameplay`), optional `on_change(id, value, source)` and `on_read(id, value, source) -> value` callables. Bad defs are listed in `errors`, the rest register. Stored values load through `options.migrate(from_version, values) -> values` when the stored schema is older than `options.version` (0 = never saved). `options.on_change` is a mod-wide change hook.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_register("my_mod", [{"id": "my_mod.gravity", "type": "float", "default": 1.0, "min": 0.1, "max": 3.0, "sync": "gameplay"}], {"version": 1})`

### `gameplay_register_setting(mod: String, def: Dictionary)` → Dictionary

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** One setting; same as `gameplay_register(mod, [def])`.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_register_setting("my_mod", def)`

### `gameplay_unregister(mod: String)` → int

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** Removes the mod's settings (values stay saved). Returns how many were removed.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_unregister("my_mod")`

### `gameplay_get(mod: String, id: String)` → Variant

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed (returns stock then)_
- **Does / side effects:** The effective value of any registered setting through the read hook: role, host override, clamp, then `on_read`. Enums are option names, vecs are Vector2/3/4. null when unknown.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `var g: float = api.gameplay_get("my_mod", "my_mod.gravity")`

### `gameplay_get_all(mod: String, owner: String = "")` → Dictionary {id: value}

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** Effective values of every setting, or only `owner`'s.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_get_all("my_mod", "my_mod")`

### `gameplay_set(mod: String, id: String, value: Variant)` → Dictionary {ok, error, value}

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Sets the local value of one of the mod's own settings (validated, snapped to step, clamped, saved). Refused for clients in a synced lobby (the host controls them) and for other mods' settings.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_set("my_mod", "my_mod.gravity", 0.5)`

### `gameplay_reset(mod: String, id: String)` → Dictionary {ok, error, value}

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Back to the default (removed from the state file).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_reset("my_mod", "my_mod.gravity")`

### `gameplay_list(mod: String)` → Array[Dictionary]

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** Every registered setting with its definition plus `value` (effective), `local`, `source` (default / local / host / stock) and `locked`.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `for s in api.gameplay_list("my_mod"): print(s.id, s.value)`

### `gameplay_info(mod: String, id: String)` → Dictionary

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** One entry of `gameplay_list`; empty when unknown.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_info("my_mod", "my_mod.gravity").source`

### `gameplay_role(mod: String)` → String

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** off (stock: gameplay closed), local (offline or the host) or client (synced lobby, not the host).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_role("my_mod")`

### `gameplay_snapshot(mod: String)` → Dictionary {ok, version, values, hash, bytes}

- **Since** 1.3 · **capability** `gameplay.settings_sync` · **access** `sync` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Host side of host sync: the effective value of every registered setting (`bytes` = the JVGS text form, `hash` = FNV-1a 64 hex). Host / offline only.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `var snap = api.gameplay_snapshot("my_sync")`

### `gameplay_snapshot_bytes(mod: String)` → PackedByteArray

- **Since** 1.3 · **capability** `gameplay.settings_sync` · **access** `sync` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** The snapshot as compact bytes (`JVGS 1` then `id=value` lines, max 32 KiB) for a lobby packet. Empty when not allowed.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `var b: PackedByteArray = api.gameplay_snapshot_bytes("my_sync")`

### `gameplay_apply_snapshot(mod: String, payload: PackedByteArray | String | Dictionary {version, values})` → Dictionary {ok, error, applied, rejected, unknown}

- **Since** 1.3 · **capability** `gameplay.settings_sync` · **access** `sync` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Client side: replaces the host's values with a received snapshot. Complete replacement (settings the host did not send read as stock); each value validated and clamped to this side's range; unknown ids ignored. Refused as host / offline.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.gameplay_apply_snapshot("my_sync", packet)`

### `gameplay_clear_host(mod: String)` → bool

- **Since** 1.3 · **capability** `gameplay.settings_sync` · **access** `sync` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** Forgets the host's values (lobby left / host changed).
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.gameplay_clear_host("my_sync")`

### `gameplay_manifest(mod: String)` → Dictionary {mods: {mod: version}, loader_ok, bytes}

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** This player's gameplay mod list for the match-wide missing-mod rule: every mod with registered gameplay settings or a gameplay / sync / offline capability, with its version (`options.mod_version`, else the settings schema, else the API version it registered with). Texture / model / loader / core UI libraries are never listed, so the rule never disables them. `loader_ok` = the texture loader (jvm_loader) is running.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_manifest("my_sync").mods`

### `gameplay_manifest_bytes(mod: String)` → PackedByteArray

- **Since** 1.3 · **capability** `gameplay.settings_sync` · **access** `sync` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** The list as bytes for a lobby packet (`JVGM 1`, `loader=0|1`, `mod=version` lines; the host's list also carries `!mod=1` for every mod it disabled for the match; max 8 KiB). Every player sends it to the host; the host sends its own to everyone after `gameplay_match_start`.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `var b = api.gameplay_manifest_bytes("my_sync")`

### `gameplay_set_peer_manifest(mod: String, player_id: String, payload: PackedByteArray | String, player_name: String = "")` → Dictionary {ok, error, player, name, mods, loader_ok, missing, late, locked, disabled}

- **Since** 1.3 · **capability** `gameplay.settings_sync` · **access** `sync` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Host: records a player's list. `missing` = the host's gameplay mods that player lacks (mod -> `missing` / `version x (host y)`); they will be off for EVERYONE from the next `gameplay_match_start`. `late` = joined while a match is locked (counted at the next match start). Logged, `gameplay_mods_status` fired, and the host gets an on-screen notice when that player's texture loader is not running. Host only.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.gameplay_set_peer_manifest("my_sync", str(peer_id), packet, peer_name)`

### `gameplay_remove_peer(mod: String, player_id: String)` → bool

- **Since** 1.3 · **capability** `gameplay.settings_sync` · **access** `sync` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** Forgets a player's list (they left). A locked match keeps its decision until it ends.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.gameplay_remove_peer("my_sync", str(peer_id))`

### `gameplay_match_start(mod: String)` → Dictionary {ok, error, disabled: {mod: [{player, name, why}]}, active, locked}

- **Since** 1.3 · **capability** `gameplay.settings_sync` · **access** `sync` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Host, at match start: decides once for the whole match. A gameplay mod that any known player lacks (or has in another major version) is disabled for everyone, host included: its settings read stock, it is left out of snapshots, the core refuses its gameplay calls, and `gameplay_mod_active_for` says false. The host gets an on-screen notice naming who is missing what. Settings are locked (no edits) until `gameplay_match_end`.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.gameplay_match_start("my_sync")`

### `gameplay_match_end(mod: String)` → bool

- **Since** 1.3 · **capability** `gameplay.settings_sync` · **access** `sync` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** Host, at match end: unlocks settings and turns every mod back on. Leaving the lobby does the same.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.gameplay_match_end("my_sync")`

### `gameplay_set_host_manifest(mod: String, payload: PackedByteArray | String)` → Dictionary {ok, error, disabled, active, missing_here, locked, host_loader_ok}

- **Since** 1.3 · **capability** `gameplay.settings_sync` · **access** `sync` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** Client: records the host's list and its match decision. Mods the host disabled (`!` lines), does not have, or has in another major version run stock here too. `missing_here` = disabled mods this player lacks; this player gets an on-screen notice for those.
- **Multiplayer:** Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.
- **Example:** `api.gameplay_set_host_manifest("my_sync", packet)`

### `gameplay_mod_active_for(mod: String, mod_id: String, player_id: String = "")` → bool

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** THE shared check for the match-wide missing-mod rule: is gameplay mod `mod_id` on in this match. The answer is the same for every player (`player_id` is accepted for callers that pass one). False while gameplay is closed, when the mod is not installed, when the host disabled it for the match, and on a client until the host's list arrived (fail closed) or when the host lacks it.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `if api.gameplay_mod_active_for("my_mod", "my_mod"): ...`

### `gameplay_match_status(mod: String)` → Dictionary {role, locked, disabled, active, would_disable}

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** The match decision: `disabled` (mod -> who lacks it), `active` mods, `locked`; on the host before match start also `would_disable` (mod -> names).
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_match_status("my_mod").disabled`

### `gameplay_player_mods(mod: String)` → Dictionary {player_id: {name, mods, loader_ok, missing, late}}

- **Since** 1.3 · **capability** `gameplay.settings` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself · _allowed while closed_
- **Does / side effects:** Per-player status: `local` (this player: mods, loader_ok, active, inactive with reasons), every player whose list the host has (missing = the host's mods they lack, late), and `host` (its list and disabled mods) on a client.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.gameplay_player_mods("my_mod")`

### `game_rules_info(mod: String)` → Dictionary {touches_per_side, consecutive_per_player, double_hit, solo_side_exempt, over_team_limit, block_counts, practice_exempt, block_classic}

- **Since** 1.3 · **capability** `game.rules` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** The stock rally rules as the game has them (from the Rules script constants where possible). Read-only.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.game_rules_info("my_mod").touches_per_side  # 3`

### `game_rules_state(mod: String)` → Dictionary {count, side, practice, has_last_toucher, last_toucher_id}

- **Since** 1.3 · **capability** `game.rules` · **access** `gameplay` · **scope** host-authoritative: the host's value reaches everyone through the game itself
- **Does / side effects:** The live rally touch state (host / offline). Empty when there is no rally.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.game_rules_state("my_mod").count`

## Shots and curve

### `shots_list(mod: String)` → PackedStringArray

- **Since** 1.1 · **capability** `shots.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Configured extra shot ids.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.shots_list("my_mod")`

### `shots_armed(mod: String)` → String

- **Since** 1.1 · **capability** `shots.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Armed shot id.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.shots_armed("my_mod")`

### `shots_last(mod: String, kind: String)` → Variant

- **Since** 1.1 · **capability** `shots.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Last spin / topspin / curve / float / event (null for other kinds).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.shots_last("my_mod", "curve")`

### `shots_spin_bar(mod: String)` → Variant

- **Since** 1.1 · **capability** `shots.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Spin bar state.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.shots_spin_bar("my_mod")`

### `shots_range_box(mod: String)` → Variant

- **Since** 1.1 · **capability** `shots.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Range box.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.shots_range_box("my_mod")`

### `shots_counts(mod: String)` → Dictionary {applied, skipped, touches}

- **Since** 1.1 · **capability** `shots.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Counters.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.shots_counts("my_mod")`

### `shots_status(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `shots.query` · **access** `read` · **scope** this PC only
- **Does / side effects:** Shots status.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.shots_status("my_mod")`

### `shot_arm(mod: String, id: String)` → bool

- **Since** 1.1 · **capability** `shots.arm` · **access** `gameplay` · **scope** this PC only
- **Does / side effects:** Arms a shot for the next contact.
- **Multiplayer:** Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.
- **Example:** `api.shot_arm("my_mod", "high_shot")`

### `shots_debug_set(mod: String, on: bool)` → bool

- **Since** 1.1 · **capability** `shots.debug` · **access** `local_ui` · **scope** this PC only · _turning off allowed while closed_
- **Does / side effects:** Debug overlay on/off.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.shots_debug_set("my_mod", true)`

### `shots_debug_visible(mod: String)` → bool

- **Since** 1.1 · **capability** `shots.debug` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Overlay visible.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.shots_debug_visible("my_mod")`

### `shots_debug_text(mod: String)` → String

- **Since** 1.1 · **capability** `shots.debug` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Overlay text.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.shots_debug_text("my_mod")`

## Library game IO

How our own libraries (goofy, shots, textures, models, the unified loader, core UI) reach the game script surface: every script property / method / signal, stock node path, autoload, engine singleton and stock resource goes through these, under a capability the library declared. Engine built-ins on objects the library owns stay local. C++ libraries use `jvm::GameIO` (src/common/api_client.hpp), which registers the library and calls these. Game `@rpc` methods are never called. `undo=true` = a revert path (allowed while the class is closed); libraries whose on/off already follows their access class (cosmetic / local UI) are declaration-checked only.

### `io_get(mod: String, cap: String, obj: Object, prop: StringName)` → Variant

- **Since** 1.1 · **capability** `(declared cap)` · **access** `?` · **scope** 
- **Does / side effects:** obj.get(prop) when `cap` is declared by `mod` (reads are declaration-checked).
- **Multiplayer:** 
- **Example:** `api.io_get("my_lib", "ball.flight", ball, "curve")`

### `io_set(mod: String, cap: String, obj: Object, prop: StringName, value: Variant, undo: bool)` → bool

- **Since** 1.1 · **capability** `(declared cap)` · **access** `?` · **scope**  · _Needs the class open unless `undo`._
- **Does / side effects:** obj.set(prop, value).
- **Multiplayer:** 
- **Example:** `api.io_set("my_lib", "ball.flight", ball, "curve", 0.0, false)`

### `io_call(mod: String, cap: String, obj: Object, method: StringName, args: Array, undo: bool)` → Variant

- **Since** 1.1 · **capability** `(declared cap)` · **access** `?` · **scope**  · _Needs the class open unless `undo`; game `@rpc` methods are refused._
- **Does / side effects:** obj.callv(method, args).
- **Multiplayer:** 
- **Example:** `api.io_call("my_lib", "ball.solver", ball, "solve_launch", [from, to, 1, 0.8, false, Vector2()], false)`

### `io_connect(mod: String, cap: String, obj: Object, signal: StringName, callable: Callable, flags: int)` → int (Error)

- **Since** 1.1 · **capability** `(declared cap)` · **access** `?` · **scope** 
- **Does / side effects:** obj.connect(signal, callable, flags). Observing is a read; the handler's own writes are checked.
- **Multiplayer:** 
- **Example:** `api.io_connect("my_lib", "ball.struck", ball, "struck", _on_struck, 0)`

### `io_disconnect(mod: String, cap: String, obj: Object, signal: StringName, callable: Callable)` → bool

- **Since** 1.1 · **capability** `(declared cap)` · **access** `?` · **scope** 
- **Does / side effects:** Disconnects when connected.
- **Multiplayer:** 
- **Example:** `api.io_disconnect("my_lib", "ball.struck", ball, "struck", _on_struck)`

### `io_node(mod: String, cap: String, from: Node, path: String)` → Node

- **Since** 1.1 · **capability** `(declared cap)` · **access** `?` · **scope** 
- **Does / side effects:** from.get_node_or_null(path): a stock node path (Court, Stage/Grass, Net ...).
- **Multiplayer:** 
- **Example:** `api.io_node("my_lib", "stage.court_look", scene, "Court")`

### `io_autoload(mod: String, cap: String, name: String)` → Node

- **Since** 1.1 · **capability** `(declared cap)` · **access** `?` · **scope** 
- **Does / side effects:** A game autoload (/root/<name>: GameConfig, Net, Achievements ...).
- **Multiplayer:** 
- **Example:** `api.io_autoload("my_lib", "ui.notify", "Achievements")`

### `io_singleton(mod: String, cap: String, name: String)` → Object

- **Since** 1.1 · **capability** `(declared cap)` · **access** `?` · **scope** 
- **Does / side effects:** An engine singleton the game registers (Steam).
- **Multiplayer:** 
- **Example:** `api.io_singleton("my_lib", "net.member_data", "Steam")`

### `io_resource(mod: String, cap: String, path: String)` → Resource

- **Since** 1.1 · **capability** `(declared cap)` · **access** `?` · **scope** 
- **Does / side effects:** A stock `res://` resource (shared, Godot-cached), e.g. the PlayerFeel tuning or a game script.
- **Multiplayer:** 
- **Example:** `api.io_resource("my_lib", "player.feel", "res://game/player/player_feel.tres")`

## Picker, Mods panel, menu perf

### `picker_ball_open(mod: String)` → bool

- **Since** 1.1 · **capability** `stage.picker` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Opens the Free Play ball picker.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.picker_ball_open("my_mod")`

### `picker_stage_inject(mod: String)` → bool

- **Since** 1.1 · **capability** `stage.picker` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Injects custom court tiles into the stage picker now.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.picker_stage_inject("my_mod")`

### `mods_panel_open(mod: String)` → bool

- **Since** 1.1 · **capability** `ui.mods_panel` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Opens our Mods panel.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.mods_panel_open("my_mod")`

### `mods_panel_close(mod: String)` → bool

- **Since** 1.1 · **capability** `ui.mods_panel` · **access** `local_ui` · **scope** this PC only · _allowed while closed_
- **Does / side effects:** Closes it.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.mods_panel_close("my_mod")`

### `mods_panel_show_list(mod: String)` → bool

- **Since** 1.1 · **capability** `ui.mods_panel` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Shows the list page.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.mods_panel_show_list("my_mod")`

### `mods_panel_show_detail(mod: String, mod_id: String)` → bool

- **Since** 1.1 · **capability** `ui.mods_panel` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Shows a mod's detail page.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.mods_panel_show_detail("my_mod", "goofy")`

### `mods_panel_info(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `ui.mods_panel` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Panel state.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.mods_panel_info("my_mod")`

### `mods_config_files(mod: String, mod_id: String)` → Variant (Array)

- **Since** 1.1 · **capability** `ui.mods_panel` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Config files of a mod category.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.mods_config_files("my_mod", "shots")`

### `mods_config_save(mod: String, path: String, text: String, live: bool)` → Variant (result)

- **Since** 1.1 · **capability** `ui.mods_panel` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Validates then saves a config file; `live` re-applies.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.mods_config_save("my_mod", p, txt, true)`

### `menu_perf_get(mod: String)` → bool

- **Since** 1.1 · **capability** `menu.perf` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Menu perf on.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.menu_perf_get("my_mod")`

### `menu_perf_set(mod: String, on: bool)` → bool

- **Since** 1.1 · **capability** `menu.perf` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Turns the cheaper menu motion on/off for this session (the saved default is `mods/core/00-default.json` `menu_perf`). Same look either way.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.menu_perf_set("my_mod", false)`

### `menu_perf_info(mod: String)` → Dictionary

- **Since** 1.1 · **capability** `menu.perf` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Counters.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.menu_perf_info("my_mod")`

### `menu_perf_reset(mod: String)` → bool

- **Since** 1.1 · **capability** `menu.perf` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Resets counters.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.menu_perf_reset("my_mod")`

## Developer tools (1.3)

Frame profiler, debugger and the player.consts checker (docs/DEVTOOLS.md). All local to this PC; nothing here changes the game. The profiler is idle (no per-frame work) until a capture starts; the debugger is off by default.

### `profiler_start(mod: String, seconds: float = 2.0, tag: String = "")` → Dictionary {ok, reason, seconds, tag}

- **Since** 1.3 · **capability** `dev.profiler` · **access** `read` · **scope** this PC only
- **Does / side effects:** Starts a capture of `seconds` (0.1-30) from the next frame: every frame's physics / process / render / rest and every CORE + library hook (texture rescan, scene cosmetics, court env / sky apply, net ticks, log writes, goofy / shots physics ...). Refused while one runs. When it ends the JSON + CSV land in `user://jv_profiler/` and the profiler goes idle again.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.profiler_start("my_mod", 2.0, "snow")`

### `profiler_toggle(mod: String, seconds: float = 0.0, tag: String = "api")` → Dictionary {ok, reason, seconds, tag, toggled}

- **Since** 1.3 · **capability** `dev.profiler` · **access** `read` · **scope** this PC only · _undo: stopping is allowed while the class is closed_
- **Does / side effects:** The same switch as the profiler hotkey and the Mods panel `dev tools: frame profiler` entry: idle -> starts a capture (`seconds` <= 0 uses `profiler_seconds`, 2 s by default), capturing -> stops it now and keeps it. `toggled` is "started" or "stopped".
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies. Undo variant works in every mode.
- **Example:** `api.profiler_toggle("my_mod")`

### `profiler_stop(mod: String)` → Dictionary (capture without frames) or {ok: false}

- **Since** 1.3 · **capability** `dev.profiler` · **access** `read` · **scope** this PC only · _undo: allowed while the class is closed_
- **Does / side effects:** Ends the running capture early and keeps it.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies. Undo variant works in every mode.
- **Example:** `api.profiler_stop("my_mod")`

### `profiler_status(mod: String)` → Dictionary {capturing, seconds, elapsed, frames, captures, last_json, last_csv, default_seconds, hotkeys, hotkeys_source, dev_build, profiler_key, console_key}

- **Since** 1.3 · **capability** `dev.profiler` · **access** `read` · **scope** this PC only
- **Does / side effects:** Profiler state and the last files written.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.profiler_status("my_mod").capturing`

### `profiler_last_capture(mod: String, frames: bool = false)` → Dictionary {meta, summary, stages, worst, hitches, stage_names, frames?}

- **Since** 1.3 · **capability** `dev.profiler` · **access** `read` · **scope** this PC only
- **Does / side effects:** The last capture. `frames` true adds one row per frame (built-in stages + `stages` {name: ms}).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.profiler_last_capture("my_mod", true).frames.size()`

### `profiler_summary(mod: String)` → Dictionary {meta, summary, stages}

- **Since** 1.3 · **capability** `dev.profiler` · **access** `read` · **scope** this PC only
- **Does / side effects:** Per-stage mean / p50 / p95 / max / total ms and calls of the last capture.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.profiler_summary("my_mod").stages["textures.rescan"].max_ms`

### `profiler_worst_frames(mod: String, n: int = 5)` → Array[Dictionary]

- **Since** 1.3 · **capability** `dev.profiler` · **access** `read` · **scope** this PC only
- **Does / side effects:** The n slowest frames, each with its full breakdown and `top` contributors.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.profiler_worst_frames("my_mod", 3)`

### `profiler_hitches(mod: String)` → Dictionary {threshold_ms, list, intervals_ms, interval_mean_ms, interval_median_ms, explained_by, per_stage}

- **Since** 1.3 · **capability** `dev.profiler` · **access** `read` · **scope** this PC only
- **Does / side effects:** Frames above max(2 x median, median + 8 ms), the time between them, and which stages are slower in hitch frames than in normal ones.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.profiler_hitches("my_mod").explained_by`

### `debug_set_enabled(mod: String, on: bool)` → bool

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only · _undo: turning it off is allowed while the class is closed_
- **Does / side effects:** Debugger on/off (off by default; mods/core `devtools.debugger`). Off clears the trace and hides the console.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies. Undo variant works in every mode.
- **Example:** `api.debug_set_enabled("my_mod", true)`

### `debug_is_enabled(mod: String)` → bool

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Debugger state.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.debug_is_enabled("my_mod")`

### `debug_set_log_level(mod: String, tag: String, level: int)` → bool

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only · _undo: level -1 (reset) is allowed while the class is closed_
- **Does / side effects:** Log level of one library tag (textures, shots, goofy, models, loader, api, gate, consts, devtools, ... or `*`): 0 debug, 1 info (default), 2 warnings only, 3 off, -1 reset. Shared by every library (Engine meta `jv_log_levels`).
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies. Undo variant works in every mode.
- **Example:** `api.debug_set_log_level("my_mod", "textures", 2)`

### `debug_log_levels(mod: String)` → Dictionary {tag: level}

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Levels set so far.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.debug_log_levels("my_mod")`

### `debug_capabilities(mod: String)` → Dictionary {capabilities, by_status, open_by_status, mode, classes, mods, libraries, api_version}

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Capability table with impl / mapped / planned and live open flags, access classes, registered mods with counters, libraries with their status and profiler taps.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.debug_capabilities("my_mod").by_status`

### `debug_tree(mod: String, path: String = "", depth: int = 1)` → Array or Dictionary

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Our own nodes only: with no path, every /root/JV* node and every JVM_* node we added to the running scene; with a path, that node (it must be ours) and `depth` levels of children.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.debug_tree("my_mod", "/root/JVModGate", 2)`

### `debug_node(mod: String, path: String)` → Dictionary {name, path, class, script, visible, processing, children, properties, meta}

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Properties (as text) and meta keys of one of our nodes; empty for game nodes.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.debug_node("my_mod", "/root/JVModGate/JVDevTools")`

### `debug_trace(mod: String, n: int = 64)` → Array[Dictionary] {t_ms, frame, kind, name, detail}

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Last n trace entries (event bus, refused calls, profiler, consts findings, match start/end). Recorded only while the debugger is on (512 kept).
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.debug_trace("my_mod", 20)`

### `debug_trace_clear(mod: String)` → bool

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only · _undo: allowed while the class is closed_
- **Does / side effects:** Empties the trace.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies. Undo variant works in every mode.
- **Example:** `api.debug_trace_clear("my_mod")`

### `debug_dump(mod: String)` → String (user:// path)

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Writes capabilities, levels, our node tree, trace, consts status and findings and profiler state to `user://jv_profiler/debug-dump-<time>.json`.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.debug_dump("my_mod")`

### `debug_console_toggle(mod: String)` → bool

- **Since** 1.3 · **capability** `dev.debugger` · **access** `local_ui` · **scope** this PC only
- **Does / side effects:** Shows / hides the in-game console (turns the debugger on). Commands: help, status, caps, mods, libs, log, levels, tree, node, trace, prof, last, worst, hitches, consts, audit, check, dump.
- **Multiplayer:** Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.
- **Example:** `api.debug_console_toggle("my_mod")`

### `consts_live(mod: String)` → Dictionary {available, reason, statics, consts, source}

- **Since** 1.3 · **capability** `player.consts_check` · **access** `read` · **scope** this PC only
- **Does / side effects:** What the player.consts bypass reports through the ONE adapter: its read API when present, else a direct read of the already-loaded player script (never loads it).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.consts_live("my_mod").statics.size()`

### `consts_status(mod: String)` → Dictionary {available, reason, source, statics, still_const, bypass_active, match_active, frozen, mode, is_host, synced_lobby, cap_open, start_set_valid, start_set, host_set_valid, host_set, approved, findings, errors, audits, checks, audit_enabled}

- **Since** 1.3 · **capability** `player.consts_check` · **access** `read` · **scope** this PC only
- **Does / side effects:** Checker state. `frozen` is true for the whole match.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.consts_status("my_mod").frozen`

### `consts_check_write(mod: String, name: String, value: Variant)` → Dictionary {ok, findings}

- **Since** 1.3 · **capability** `player.consts_check` · **access** `read` · **scope** this PC only
- **Does / side effects:** Judges a write before it happens (the player.consts setter calls this; mods may too). Errors: CONST_NOT_STATIC, UNKNOWN_NAME, NOT_GAMEPLAY, PROTOCOL_NAME, BAD_TYPE, BAD_VALUE, OUT_OF_RANGE, CAP_UNAVAILABLE, NON_HOST_WRITE, MID_MATCH_WRITE (any write once the match started). An accepted value becomes the expected one for the audit.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.consts_check_write("goofy", "SET_DUMP_RISE_TAP", 3.0).ok`

### `consts_audit(mod: String)` → Dictionary {ok, findings, bypass_active, source}

- **Since** 1.3 · **capability** `player.consts_check` · **access** `read` · **scope** this PC only
- **Does / side effects:** Checks the live values now (also runs at match start and once a second while the bypass is active): MID_MATCH_CHANGE, HOST_MISMATCH, UNTRACKED_WRITE, NOT_STOCK_WHILE_CLOSED, OUT_OF_RANGE, PROTOCOL_REWRITTEN, CONST_NOT_REWRITTEN, FOLDED_COPY_STALE (warning).
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.consts_audit("my_mod").findings`

### `consts_findings(mod: String)` → Array[Dictionary] {severity, code, name, detail, source, count, first_ms, last_ms}

- **Since** 1.3 · **capability** `player.consts_check` · **access** `read` · **scope** this PC only
- **Does / side effects:** Every finding so far (deduplicated with counts, 500 kept). Errors are also logged once as `[jv-mods:consts] WARNING ...`.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.consts_findings("my_mod")`

### `consts_clear_findings(mod: String)` → bool

- **Since** 1.3 · **capability** `player.consts_check` · **access** `read` · **scope** this PC only · _undo: allowed while the class is closed_
- **Does / side effects:** Forgets the findings.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies. Undo variant works in every mode.
- **Example:** `api.consts_clear_findings("my_mod")`

### `consts_set_host_values(mod: String, values: Dictionary)` → bool

- **Since** 1.3 · **capability** `player.consts_check` · **access** `read` · **scope** this PC only
- **Does / side effects:** Client side: the host's start-of-match set as received by the sync layer; live values that differ are HOST_MISMATCH errors. On the host (and offline) the start-of-match capture is the host set.
- **Multiplayer:** Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.
- **Example:** `api.consts_set_host_values("goofy", host_values)`

## Signals

### `access_changed(access: String, open: bool, reason: String)`
- **Since** 1.0. Fires the same frame an access class opens/closes (also on the event bus as `access_changed`). Undo your changes when `open` is false. Fires with everything closed when the core is removed.
- **Example:** `api.access_changed.connect(func(a, o, r): if a == "offline" and not o: api.goofy_clear_override("my_mod"))`

### `event(name: String, data: Dictionary)`
- **Since** 1.1. Every event of the bus, for anyone (no registration). Use `subscribe` to get only some.
- **Example:** `api.event.connect(func(n, d): print(n, d))`

### `gameplay_setting_changed(id: String, value: Variant, source: String)`
- **Since** 1.3. A registered gameplay setting's effective value changed (also on the event bus as `gameplay_setting_changed`). `source`: default, local, host or stock.
- **Example:** `api.gameplay_setting_changed.connect(func(id, v, src): if id == "my_mod.gravity": apply(v))`

## Events

| event | capability | data | when |
|---|---|---|---|
| `access_changed` | `gate.mode` | `{access, open, reason}` | an access class opens or closes (same as signal access_changed) |
| `mode_changed` | `gate.mode` | `{mode, previous}` | the detected play mode changes (Free Play -> LAN, menu -> public, ...) |
| `scene_changed` | `scene.current` | `{scene, name, previous_name}` | the running scene changes (menu <-> match <-> practice) |
| `stage_picked` | `stage.pick` | `{id, previous}` | the picked custom court changes ("" = a stock stage) |
| `ball_picked` | `ball.pick` | `{id, previous}` | the picked custom ball changes ("" = stock) |
| `loader_map_changed` | `loader.query` | `{id, previous}` | the unified loader's current map changes ("" = stock map) |
| `model_changed` | `models.query` | `{id, previous}` | the shown player model changes (saved pick or preview) |
| `menu_perf_changed` | `menu.perf` | `{enabled}` | the menu perf toggle changes |
| `goofy_params_changed` | `goofy.query` | `{changed, effective}` | Goofy's effective ball params change (relayed from Goofy API 1.0 params_changed) |
| `goofy_ball_mode_changed` | `goofy.query` | `{mode, force_defaults}` | Ball override NORMAL/FIXED changes (relayed from ball_mode_changed) |
| `goofy_physics_allowed_changed` | `goofy.query` | `{allowed}` | Goofy physics become allowed/blocked (relayed from physics_allowed_changed) |
| `goofy_ball_event` | `goofy.events` | `{name, index, args}` | a ball signal fired: bounced, struck(pos, power, kind), net_hit, wall_hit, antenna_hit(side); index = goofy_balls() index |
| `goofy_player_event` | `goofy.events` | `{name, index, args}` | a player signal fired: touched, swung, thumped, knock_started, touch_attempted, out_of_range, launched, jumped, callout_made, high_fived, block_broken, dove_into_net(_pole); index = goofy_players() index |
| `goofy_sync_changed` | `goofy.query` | `{role, host_values}` | Goofy host sync role (off/offline/host/client) or the host's values changed |
| `gameplay_setting_changed` | `gameplay.settings` | `{id, value, previous, source}` | a registered gameplay setting's EFFECTIVE value changed (local edit, host values arrived, or the role changed: offline / host / client / off) |
| `gameplay_role_changed` | `gameplay.settings` | `{role, previous}` | the registry's role changed: off (stock), local (offline / host), client (host's values) |
| `gameplay_mods_status` | `gameplay.settings` | `{player, name, disabled, active, missing, late, locked, ...}` | the match-wide missing-mod rule ran: a player's mod list reached the host (player id, missing = host mods they lack, late = joined mid-match), the host decided at match start (player "match", disabled = mod -> who lacks it: off for everyone), or a client got the host's list (player "local") |

## Properties

JVModAPI exposes no settable properties: every state change goes through a checked function so it can be gated, counted and undone. Read-only facts are functions (`get_api_version`, `get_mode`, `is_strict`, ...). The node path `/root/JVModGate/JVModAPI` and the singleton name `JVModAPI` are stable for major 1. `status.json` (user://jv_mods/status.json) carries an `api` summary: `version`, `strict`, `mods`, `coverage`.

## Config keys and files

| file / key | type | default | meaning |
|---|---|---|---|
| `mods/api.json` `strict` | bool | false | Strict API mode: only libraries that reach the game through CORE load. Every shipped library is migrated (goofy, shots, loader = textures + models; core UI is part of core), so strict mode refuses only third-party or legacy GDExtension libraries. |
| `mods/api/state/<mod>.json` | JSON object | absent | Per-mod state written by `state_set` (atomic). |
| `mods/api_mods/<name>/mod.gd` | GDScript (extends Node) | absent | A script mod; loaded after the version pin matched, parented under `/root/JVScriptMods/<name>`. |
| `mods/api_mods/<name>/mod.json` `enabled` | bool | true | false = skip this script mod. |
| `mods/core/00-default.json` `devtools` | object | see docs/DEVTOOLS.md | `debugger` (false), `hotkeys` ("auto" = build default, or true / false; `profiler_key` F6, `console_key` F11), `overlay` (true), `profiler_seconds` (2), `consts_audit` (true), `log_levels` {tag: level}. |
| `user://jv_profiler/` | folder | - | Profiler captures (`capture-<time>-<n>[-tag].json` + `.csv`) and `debug-dump-<time>.json`. |
| `mods/core/00-default.json` `menu_perf` | bool | true | Menu perf default (cheaper menu motion, same look). `menu_perf_set` changes it for the session. |
| `mods/<category>/*.json` | JSON | per library | Library configs (shots, goofy, textures ...), read by `load_config` / `library_reload`, validated by `config_validate`. |
| stage pack `skin.json` `audio{}` | JSON | - | Optional native ambience for a court (`ambience` = native id/path/alias such as `beach`, plus optional `bus` / `volume`). Parsed only; the stream is loaded when the pack is applied. See docs/PICKERS.md. |
| stage pack `skin.json` `environment{}` / `sky{}` / `court{}` / `goofy{}` | JSON | - | Court environment GLB, distant sky, court look and the map's goofy block (see docs/PICKERS.md, docs/UNIFIED-LOADER.md). Read via `court_env_info`, `loader_map_goofy_block`. |

## Capabilities

| id | since | access | scope | status | feature |
|---|---|---|---|---|---|
| `api.info` | 1.0 | read | local | impl | API version, capability list and live gate status |
| `api.mods` | 1.0 | read | local | impl | Mod registration: each mod declares the capabilities it uses; undeclared calls are refused |
| `gate.mode` | 1.0 | read | local | impl | Detected play mode (Free Play/practice/offline/LAN/private/friends/public/quick match/menu) and which access classes are open |
| `config.files` | 1.0 | local_ui | local | impl | Read the mods folder JSON configs (mods/<category>/*.json merged by file name) |
| `scene.current` | 1.0 | read | local | impl | The running game scene (main match scene, menu, practice court) |
| `scene.balls` | 1.0 | read | local | impl | Every ball in the scene (class Ball) |
| `scene.players` | 1.0 | read | local | impl | Every player in the scene (class Player) and the local player |
| `scene.rally` | 1.0 | read | local | impl | The rally judge that owns a player (main scene, also embedded in the practice court) |
| `scene.stage` | 1.0 | read | local | impl | Current stage/court nodes (Stage, Court, CourtFloor, Grass, Net, Backdrop) |
| `net.state` | 1.0 | read | local | impl | Online session state: active, host, Steam lobby id, LAN party, custom lobby, privacy, match / lobby practice running |
| `net.lobby_data` | 1.0 | sync | peers | impl | Steam lobby data keys owned by mods (host writes, members read), e.g. jvm_rules |
| `net.member_data` | 1.0 | sync | peers | mapped | Steam lobby MEMBER data (texture sync fallback key jvm_skins) |
| `net.packets` | 1.0 | sync | peers | mapped | Raw SceneMultiplayer bytes between modded peers (JVMS packets; never the game's own RPCs) |
| `net.party_ball` | 1.0 | sync | host | mapped | Send the picked ball skin to the party (game's own function) |
| `net.game_signals` | 1.0 | read | local | impl | Observe the game's network signals (lobby_changed, match_started, peer_left_match, ...); the game's @rpc functions are never called by mods |
| `net.content_offer` | 1.2 | sync | peers | impl | Cosmetic sharing: every modded peer advertises what it wears (outfit, accessories, ball, court, model: id + SHA-256 + size) in JVMS packets on join and on every change; others render it (ball / court follow the host) |
| `net.content_download` | 1.2 | sync | peers | impl | Missing advertised items are fetched from the embedded assets repo only (manifest v1; pack.zip SHA-256 + size checked, PNG / JSON / glb only, no path traversal), cached under mods/resources/cache and loaded live; never code; works offline |
| `ball.physics` | 1.0 | gameplay | local | impl | The shared BallPhysics resource (radius = real hitbox, gravity, floor_bounce, any exported field) |
| `ball.model` | 1.0 | read | local | impl | The ball's dressed visual model (sized from PHYS.radius when dressed) |
| `ball.scale` | 1.0 | gameplay | local | impl | Ball node scale (visual size to match a changed radius) |
| `ball.flight` | 1.0 | gameplay | local | impl | Ball flight state: velocity, curve, dive gravity, strike sequence, hitter |
| `ball.float` | 1.0 | gameplay | local | mapped | Float serve/shot wobble (start_float, stop_float, float_seed_for, solve_float) |
| `ball.solver` | 1.0 | read | local | mapped | The game's own trajectory maths (solve_launch, step_flight, landing prediction) |
| `ball.struck` | 1.0 | read | local | impl | Ball contact event struck(pos, power, kind), emitted at the end of Ball.hit |
| `ball.events` | 1.0 | read | local | impl | Other ball events: bounced, net_hit, wall_hit, antenna_hit |
| `ball.skin` | 1.0 | cosmetic | local | impl | Ball look: the game's ball skins and our custom ball material (equirect texture) |
| `ball.landing_marker` | 1.0 | cosmetic | local | mapped | Landing marker / spread ring and the float-box shards used by the shot indicators |
| `player.touched` | 1.0 | read | local | impl | Player touch event touched(kind), after the rally judged it |
| `player.events` | 1.0 | read | local | impl | Other player events: swung, launched, jumped, callout_made, high_fived, block_broken, dove_into_net |
| `player.scale` | 1.0 | gameplay | local | impl | Player node scale (goofy player_scale) |
| `player.input_state` | 1.0 | read | local | impl | What the player is doing: serving, contact state, aim point, hold power, grounded, team side |
| `player.input` | 1.0 | read | local | mapped | Input actions and the game's input collector (camera-relative move, hitter frame) |
| `player.feel` | 1.0 | read | local | impl | PlayerFeel tuning resource (read by the shots maths) |
| `player.model` | 1.0 | cosmetic | local | impl | Custom full player model grafted onto the game's rig (model_hook) |
| `player.look` | 1.0 | cosmetic | local | mapped | Player look data: skin colour, jersey, hands |
| `player.outfit` | 1.0 | cosmetic | local | mapped | Outfit textures per surface slot (jersey/shorts/shoes) and 3D accessories on bones |
| `rules.touch_count` | 1.0 | gameplay | host | impl | Touches per side: read the rally's count/side, rewrite the count (host enforces; clients get it in player state) |
| `rules.double_hit` | 1.0 | gameplay | host | impl | Allow one player to touch twice in a row (clear last_toucher before the CONSECUTIVE check) |
| `rules.modifiers_panel` | 1.0 | gameplay | local | impl | Rows in the game's custom-lobby Modifiers panel |
| `engine.time_scale` | 1.0 | gameplay | local | impl | Engine.time_scale (slow-mo / fast-mo), this PC; in LAN / private / friends matches Goofy host sync applies the host's value |
| `engine.overrides` | 1.0 | gameplay | local | impl | Reversible property overrides on game nodes picked by group/path/script class (goofy 'overrides') |
| `game.generic` | 1.0 | gameplay | local | impl | Escape hatch for everything in docs/GAME_SURFACE.md without a semantic function yet: get/set a property, call a method, connect a signal. Logged per mod; never the game's @rpc functions |
| `stage.court_look` | 1.0 | cosmetic | local | impl | Court floor, grass, hills and backdrop materials (court reskins) |
| `stage.net_look` | 1.0 | cosmetic | local | impl | Net tape/cord/post/pad colours and textures |
| `stage.picker` | 1.0 | local_ui | local | impl | Free Play stage + ball picker tiles (custom courts/balls) |
| `stage.lobby_picker` | 1.0 | local_ui | host | planned | Lobby stage list (custom courts in lobbies, host pick for everyone) |
| `ui.menu` | 1.0 | local_ui | local | mapped | Main menu card and bottom-bar injection (Mods button, credits) |
| `ui.style` | 1.0 | read | local | mapped | The game's UiStyle builders (style_button, card_box, rail_tile, font, icon, sfx) |
| `ui.locker` | 1.0 | local_ui | local | mapped | Locker tabs (full model tab, accessory slots) |
| `ui.notify` | 1.0 | local_ui | local | mapped | The game's toast notifications |
| `lib.status` | 1.1 | read | local | impl | Which mod libraries/subsystems are installed and their status (loader, textures, models, shots, goofy, menu UI, menu perf, picker) |
| `lib.reload` | 1.1 | local_ui | local | impl | Re-read a library's config files from the mods folder (same as the Mods panel reload) |
| `config.state` | 1.1 | local_ui | local | impl | Per-mod persistent key/value state (mods/api/state/<mod>.json, atomic write) |
| `config.validate` | 1.1 | read | local | impl | Validate a config dictionary against a library's schema (shots, goofy) |
| `events.bus` | 1.1 | read | local | impl | Event bus: subscribe to API events (mode, scene, picks, loader map, model, menu perf, goofy relays); each event also needs its own capability |
| `packs.query` | 1.1 | read | local | impl | Pack registry (stage / ball / skin packs), current picks, texture status, sync status and texture cache |
| `packs.rescan` | 1.1 | local_ui | local | impl | Re-read the pack folders so new packs appear (unchanged images come from the cache) |
| `stage.pick` | 1.1 | cosmetic | local | impl | Pick a custom court (stage pack) and apply picked cosmetics now; refresh the unified loader |
| `ball.pick` | 1.1 | cosmetic | local | impl | Pick a custom ball (ball pack) |
| `court.query` | 1.1 | read | local | impl | Court environment (environment{} GLB around the court) and distant sky (sky{}) status and a pack's blocks |
| `court.env` | 1.1 | cosmetic | local | impl | Load a court with its environment GLB + sky (pick + apply; clearance-checked by the loader) |
| `audio.query` | 1.4 | read | local | impl | List the game's native sound effects and ambience tracks (cheap path index only; no streams, players or nodes until something plays) and the live audio buses |
| `audio.play` | 1.4 | cosmetic | local | impl | Play or loop a native game sound, stop it, and set per-voice volume or bus; playback uses the game's Master/sfx/music/ambient/crowd/announcer buses and is created only on request |
| `accessories.query` | 1.1 | read | local | impl | Locker accessories from packs (Head / Face / Extra), what is worn, unplaced ids |
| `models.query` | 1.1 | read | local | impl | Custom player models: list, saved pick, shown model, preview state, per-model settings, rig report |
| `loader.query` | 1.1 | read | local | impl | Unified loader: status, goofy bridge, loader maps, a map's goofy block |
| `goofy.query` | 1.1 | read | local | impl | Goofy ball + player libraries: schemas (ball 156: BallPhysics x124, PlayerFeel contact x27, flight x5; player 195), effective/stock values, layers, Ball override mode, rules, live ball / player state, cosmetics state, host sync status |
| `goofy.ball` | 1.1 | offline | local | impl | Set / clear this mod's Goofy BALL layer (any of the 156 ball-library params; owner mod:<name>, priority 0..899). Live only offline; kept and re-applied when offline again |
| `goofy.packs` | 1.1 | gameplay | local | impl | Loader-map goofy sections handed to the goofy library (its pack layer + loader map); goofy itself decides when they are live |
| `goofy.ball_mode` | 1.1 | local_ui | local | impl | Ball override NORMAL / FIXED on loader maps (saved preference) |
| `goofy.rules` | 1.1 | gameplay | host | impl | Goofy rules: max hits per team and no-double-hit (host-enforced, shown in the Modifiers panel) |
| `player.tuning` | 1.2 | gameplay | local | impl | PlayerFeel writes: movement, jump, sprint / stamina, dive, block, serve, animation and contact timing (the Goofy player library; host-synced in LAN / private / friends) |
| `player.consts` | 1.2 | gameplay | local | impl | Player consts: the 34 gameplay values hard-coded in player.gd (made static vars at boot, match-wide). Edit in the lobby only; frozen for the whole match (mid-match writes return an error); the host sends its set to every player at match start. REQUIRES player.consts (same game build) on EVERY player: if anyone lacks it the whole match plays stock and the host is told who |
| `goofy.player` | 1.2 | offline | local | impl | Set / clear this mod's Goofy PLAYER layer (player library params incl. player.body_scale; owner mod:<name>:player). Live offline; kept and re-applied when offline again |
| `goofy.ball_live` | 1.2 | offline | local | impl | Live writes on a ball in flight: velocity, curve, impulse (unsynced, so offline only) |
| `goofy.events` | 1.2 | read | local | impl | Ball and player game events relayed by Goofy with ball / player index (bounced, struck, net_hit, ..., swung, jumped, launched, dove_into_net, ...) |
| `goofy.host_sync` | 1.2 | sync | host | impl | Host-authoritative Goofy gameplay: the host publishes its gameplay ball + player params and time scale to modded peers (JVGF packets v2, clamped on receive); clients apply them from match start to match end, visual-only params stay local |
| `gameplay.settings` | 1.3 | gameplay | host | impl | Gameplay settings registry and read hook: mods register settings that change play (id mod.setting, type, default, range, label, tooltip, category; sync class gameplay only, visual-only refused) at config setup; every read goes through gameplay_get (stock while gameplay is closed, the host's value for clients in LAN / private / friends lobbies, else the local value; always clamped); values saved per mod with versioned migration; controls auto-built in the game's Settings panel (mods tab) |
| `gameplay.settings_sync` | 1.3 | sync | host | impl | Host broadcast hooks for the gameplay settings registry: the host exports its effective values (dictionary / JVGS bytes / hash), clients apply the host's payload (validated, unknown ids ignored, clamped); match-wide missing-mod rule: players' gameplay mod lists, decided at match start (a mod any player lacks is off for everyone, host included; texture mods never), settings locked for the match, loader warning; the transport belongs to the host-sync library |
| `game.rules` | 1.3 | gameplay | host | impl | Game rules on the host's rally judge (clients never judge): team touches (four-touches fault, point to the other team), one player's consecutive touches (CONSECUTIVE bar), block / body block reset, PlayerFeel.block_classic; stock restored on revert |
| `shots.query` | 1.1 | read | local | impl | Extra shots / curve: shot ids, armed shot, last spin / topspin / curve / float / event, spin bar, range box, counters, status |
| `shots.arm` | 1.1 | gameplay | local | impl | Arm an extra shot for the next contact |
| `shots.debug` | 1.1 | local_ui | local | impl | Shots debug overlay on/off and its text |
| `ui.mods_panel` | 1.1 | local_ui | local | impl | Our Mods panel in the main menu: open/close, list/detail page, config files, validate + save a config |
| `menu.perf` | 1.1 | local_ui | local | impl | Menu perf toggle (cheaper menu motion, same look; mods/core/00-default.json menu_perf) and its counters |
| `dev.profiler` | 1.3 | read | local | impl | Frame profiler: capture N seconds (default 2) of frames with per-stage timings (physics, process, render, every CORE / library hook), JSON + CSV under user://jv_profiler/, summaries, worst frames, hitches |
| `dev.debugger` | 1.3 | local_ui | local | impl | CORE debugger (off by default): per-library log levels, capability / library status dump, inspect our own nodes, event trace, in-game console |
| `player.consts_check` | 1.3 | read | local | impl | Altered-player-script checker for the player.consts bypass: flags writes to names still const, out of clamp / type, never-change names, non-host writes in synced lobbies, writes while unavailable, any mid-match write (values frozen for the match), client values differing from the host's start-of-match set, and folded copies in other scripts |

Coverage: 88 capabilities, 74 implemented, 13 mapped (reachable through `game.generic`, documented with file:line), 1 planned. Mapped ones are listed in docs/API.md's migration checklist.

## Script mods and the example

`examples/api_mods/hello_api/` is a complete script mod (not installed by the release zip). Copy it to `<game>/mods/api_mods/hello_api/`. It registers, subscribes to events, counts ball contacts through `connect_ball_event`, keeps the count in its state file, and in Free Play puts a gentle low-gravity Goofy layer on that it clears the moment the offline class closes. The pattern:

```gdscript
var api = Engine.get_singleton("JVModAPI")
api.register_mod("my_mod", "1.1", PackedStringArray(["gate.mode", "goofy.query", "goofy.ball"]))
api.access_changed.connect(func(a, open, _r):
    if a == "offline" and not open: api.goofy_clear_override("my_mod"))
if api.is_open("offline"): api.goofy_set_override("my_mod", {"ball.gravity": {"mul": 0.85}})
```

## Writing a C++ mod

```cpp
#include "../common/api_client.hpp"   // jvm::find_api(), ApiFollower<T>
Object *api = jvm::find_api(this);
api->call("register_mod", "my_lib", "1.1", PackedStringArray({"packs.query", "stage.pick"}));
String picked = api->call("stage_get_picked", "my_lib");
```
Every shipped library is migrated: `ApiFollower` decides apply / revert from the library's access class and `jvm::GameIO` is its only path to the game:

```cpp
static jvm::GameIO &IO() { static jvm::GameIO io = [] { jvm::GameIO g; g.mod = "my_lib"; g.caps.push_back("ball.flight"); return g; }(); return io; }
Variant curve = IO().get("ball.flight", ball, "curve");          // was ball->get("curve")
IO().set("ball.flight", ball, "curve", 0.0);                     // needs the class open
IO().call_undo("ball.landing_marker", ball, "_shard_cluster_mesh", 0.0, 1.0); // revert path
Node *court = IO().node("stage.court_look", scene, "Court");    // stock node path
String shown = IO().core("model_get_shown");                     // another library, through its CORE facade
```

| library | access class (on/off) | game IO writes | declared capabilities |
|---|---|---|---|
| goofy | gameplay | API facades | goofy.* (see GOOFY-API.md) |
| shots | gameplay | need the class open (revert paths use `undo`) | gate.mode, config.files, scene.*, ball.flight, ball.physics, ball.float, ball.solver, ball.landing_marker, ball.struck, player.input_state, player.input, player.feel |
| textures (court env / sky, custom ball, locker) | cosmetic | declaration-checked | scene.*, net.state, net.member_data, net.packets, ball.model, ball.physics, ball.skin, player.model, player.look, player.outfit, stage.court_look, stage.net_look, court.env, models.query, ui.locker, ui.menu |
| models (model loader, locker tab) | cosmetic | declaration-checked | scene.*, player.model, player.look, ui.locker, ui.style |
| loader (unified loader bridge) | follows its subsystems | declaration-checked | scene.stage, packs.query, goofy.packs |
| core_ui (menu card, picker, mods panel, menu perf) | local UI (core) | declaration-checked | ui.menu, ui.style, ui.notify, ui.mods_panel, menu.perf, stage.picker, stage.pick, ball.pick, ball.skin, packs.*, net.party_ball, lib.* |

Refusals are logged once per mod and capability (`refused <mod> -> <cap>: <why>`) and counted in `get_mods()`.

## Decisions

* `engine.time_scale` (time_scale_set / time_scale_restore) stays a gameplay-class, local Engine.time_scale write for mods. In LAN / private / friends **matches** the time scale follows the **host** (owner decision): Goofy host sync sends the host's time scale with all gameplay params at match start, each client applies it through `time_scale_set`, re-asserts it after the game's own hit-stop slow-mo, and restores its own value at match end or when the host leaves. Public lobbies: local only, nothing sent.
* Everything that alters the ball or a player follows the host in a synced match unless it is visual-only; the per-param classification (`sync`: host / local) is in `goofy_ball_schema` / `goofy_player_schema` and docs/GOOFY-BALL.md, GOOFY-PLAYER.md. Visual-only params and cosmetics stay each player's own.
* Cosmetics are per player and never host-synced: each modded peer advertises what it wears (id + SHA-256 + size, JVMS packets) on join and on every change, everyone renders everyone's outfit, accessories and model; the court and ball everyone plays on are the host's pick. Missing items come only from the embedded assets repo (docs/CONTENT-SHARING.md). Sharing is LAN / private / friends only; public lobbies stay local (no packets, no downloads).
* There is no separate space-platform gravity or underwater drag in the game or Goofy; ball / player gravity and drag are ordinary gameplay params and follow the host.

