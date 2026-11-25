#!/usr/bin/env python3
"""Generate docs/CORE-API.md: every JVModAPI function, signal, property and config key with
parameters, return value, side effects, multiplayer safety and an example.

Capability access / scope come from src/api/capabilities.def (single source of truth), so the
multiplayer-safety line can never drift from what the code enforces. tests/test_api.cpp checks
that every bound function (D_METHOD / BM in src/core/mod_api*.cpp) appears here.
Usage: python3 tools/gen_core_api_doc.py   (rewrites docs/CORE-API.md)
"""
import os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

caps = {}
for m in re.finditer(r'JV_CAP\("([^"]+)", "([^"]+)", (\w+), (\w+), (\w+), "((?:[^"\\]|\\.)*)"', open(os.path.join(ROOT, "src/api/capabilities.def")).read()):
    caps[m.group(1)] = dict(since=m.group(2), access=m.group(3).lower(), scope=m.group(4).lower(), status=m.group(5).lower(), feature=m.group(6))

MP = {
    "read": "Safe everywhere (read-only, this PC). Open once the game version pin matched, also in public lobbies.",
    "local_ui": "Safe everywhere: only our own menus/files on this PC, never gameplay. Open once pinned, also in public lobbies.",
    "cosmetic": "Local visuals only (other players never see or receive it). Open in every identified mode incl. public / quick match; closed on an unknown context.",
    "gameplay": "Changes play. Open in Free Play / practice / offline solo / menu and in LAN / private / friends-only lobbies; refused in public, quick match, server browser and unknown.",
    "sync": "Talks to other modded players. Open ONLY in LAN / private / friends-only lobbies.",
    "offline": "Unsynced engine-wide change: open ONLY in Free Play / practice / offline solo / menu; refused in every online lobby (LAN/private/friends too) so nothing desyncs.",
}
SCOPE = {"local": "this PC only", "host": "host-authoritative: the host's value reaches everyone through the game itself", "peers": "sent to modded peers"}


F = []
def sec(title, intro, rows): F.append((title, intro, rows))

sec("API info, registration, gate", "Call these first. `register_mod` is required before any call that names a mod.", [
 ("get_api_version", "", "String", "-", "", "API version `major.minor.patch` (now `1.4.0`). No side effects.", 'api.get_api_version()  # "1.4.0"'),
 ("register_mod", "name: String, version: String, caps: PackedStringArray", "Dictionary {ok, reason, granted, denied}", "-", "", "Registers (or re-registers) a mod with the API version it was written for (same major, minor <= ours) and the capabilities it will call. Unknown and planned ids are denied (listed in `denied`). Logged once.", 'api.register_mod("my_mod", "1.1", PackedStringArray(["scene.balls", "goofy.ball"]))'),
 ("get_capabilities", "", "Array[Dictionary] {id, since, access, scope, status, feature, game_refs, functions, open}", "-", "", "The whole capability table with the live `open` flag.", "for c in api.get_capabilities(): print(c.id, c.open)"),
 ("has_capability", "id: String", "bool", "-", "", "True when the id exists and is not planned.", 'api.has_capability("court.env")'),
 ("can", "mod: String, cap: String", "bool", "-", "", "Would a call under `cap` by `mod` be allowed right now (declared + gate). Does not count as a call.", 'if api.can("my_mod", "goofy.ball"): ...'),
 ("why_not", "mod: String, cap: String", "String", "-", "", "Empty when allowed, else the reason (\"offline only: ... (mode: LAN)\").", 'print(api.why_not("my_mod", "goofy.ball"))'),
 ("is_open", "access: String", "bool", "-", "", "Is an access class open now: read, local_ui, cosmetic, gameplay, sync, offline.", 'api.is_open("offline")'),
 ("get_reason", "access: String", "String", "-", "", "Human reason for the class state (gate text).", 'api.get_reason("gameplay")'),
 ("get_mode", "", "String", "-", "", "Detected play mode: Free Play, practice, offline solo, menu, LAN, private, friends-only, public, quick match, server browser, unknown.", "api.get_mode()"),
 ("gate_matrix", "", "Dictionary {mode: {access: bool}}", "-", "", "The full mode x access-class table the gate enforces (rules.md). Pure; no side effects.", 'api.gate_matrix()["LAN"]["sync"]  # true'),
 ("net_safety", "cap: String", "Dictionary {found, access, scope, open_in, public_safe, reaches_other_players}", "-", "", "Multiplayer profile of a capability: in which modes it opens and whether it reaches other players.", 'api.net_safety("goofy.ball").open_in'),
 ("get_status", "", "Dictionary {version, strict, mode, open, mods, coverage}", "-", "", "Live API status (also summarised in status.json under `api`).", "api.get_status()"),
 ("get_coverage", "", "Dictionary {capabilities, impl, mapped, planned, game_refs, used_by_mods, by_access}", "-", "", "Coverage numbers of the capability table.", "api.get_coverage()"),
 ("get_mods", "", "Dictionary {mod: {version, declared, calls, refusals}}", "-", "", "Registered mods with per-capability call / refusal counters.", 'api.get_mods()["goofy"].declared'),
 ("library_allowed", "short_name: String", "bool", "-", "", "Strict mode check used by bootstrap.gd (`mods/api.json {\"strict\": true}`): only libraries migrated to the API load.", 'api.library_allowed("shots")'),
 ("is_strict", "", "bool", "-", "", "Strict API mode on/off (reads mods/api.json once).", "api.is_strict()"),
 ("script_mods_list", "", "Array[Dictionary] {name, path}", "-", "", "Script mods loaded from `mods/api_mods/<name>/mod.gd`.", "api.script_mods_list()"),
 ("load_script_mods", "", "void", "-", "", "Internal: called once (deferred) by the API's `_ready` to load script mods when the version pin matched. Calling it again loads nothing new unless new folders appeared (they get new nodes).", "# called by the core"),
])
sec("Config and state", "", [
 ("load_config", "mod: String, category: String", "Dictionary", "config.files", "", "Merged `mods/<category>/*.json` (sorted by file name, later wins). Read-only.", 'api.load_config("my_mod", "goofy")'),
 ("state_get", "mod: String, key: String, default: Variant", "Variant", "config.state", "", "Value from this mod's state file `mods/api/state/<mod>.json`, or `default`.", 'api.state_get("my_mod", "hits", 0)'),
 ("state_set", "mod: String, key: String, value: Variant", "bool", "config.state", "", "Writes the key (JSON types) with an atomic tmp+rename write. Creates the folder.", 'api.state_set("my_mod", "hits", 12)'),
 ("state_erase", "mod: String, key: String", "bool", "config.state", "undo: allowed while the class is closed", "Removes the key; false when absent.", 'api.state_erase("my_mod", "hits")'),
 ("state_keys", "mod: String", "PackedStringArray", "config.state", "", "Keys in this mod's state file.", 'api.state_keys("my_mod")'),
 ("config_validate", "mod: String, library: String, config: Dictionary", "Dictionary {ok, errors}", "config.validate", "", "Validates a config dictionary with a library's own validator (`shots`, `goofy`). No side effects.", 'api.config_validate("my_mod", "shots", cfg)'),
 ("libraries_status", "mod: String", "Dictionary {lib: {present, has_status}, gate_libraries}", "lib.status", "", "Which subsystems are installed: loader, textures, models, shots, goofy, menu_ui, menu_perf, picker, plus the gate's per-library load state.", 'api.libraries_status("my_mod").goofy.present'),
 ("library_status", "mod: String, library: String", "Dictionary", "lib.status", "", "That library's own status dictionary (`get_status` / `get_info`). Empty when absent.", 'api.library_status("my_mod", "shots")'),
 ("library_reload", "mod: String, library: String", "bool", "lib.reload", "", "Re-reads the library's config files (same as the Mods panel reload); the library re-applies under its own gate.", 'api.library_reload("my_mod", "goofy")'),
])
sec("Events and signals", "Events are delivered both as the API signal `event(name, data)` and to `subscribe` callables `fn(name: String, data: Dictionary)`. Subscribing needs `events.bus` plus the event's own capability. State-change events are polled at 4 Hz from cheap getters (no tree walks); goofy events are relayed from Goofy API 1.0 signals the moment they fire.", [
 ("events_list", "", "Array[Dictionary] {name, capability, data, when}", "-", "", "Every event the bus can deliver (table below).", "api.events_list()"),
 ("subscribe", "mod: String, event: String, callable: Callable", "bool", "events.bus", "", "Adds a subscriber (idempotent). Freed callables are dropped automatically.", 'api.subscribe("my_mod", "mode_changed", func(n, d): print(d.mode))'),
 ("unsubscribe", "mod: String, event: String, callable: Callable", "bool", "-", "", "Removes the subscriber; false if it was not subscribed.", 'api.unsubscribe("my_mod", "mode_changed", cb)'),
 ("connect_ball_event", "mod: String, ball: Node, name: String, callable: Callable", "bool", "ball.events", "`struck` uses ball.struck", "Connects to a Ball signal by name: bounced, struck, net_hit, wall_hit, antenna_hit (ball.gd:13-19). Anything else (incl. rpc) refused.", 'api.connect_ball_event("my_mod", b, "struck", _on_struck)'),
 ("connect_player_event", "mod: String, player: Node, name: String, callable: Callable", "bool", "player.events", "`touched` uses player.touched", "Connects to a Player signal: touched, swung, thumped, knock_started, touch_attempted, out_of_range, launched, jumped, callout_made, high_fived, block_broken, dove_into_net, dove_into_net_pole (player.gd:126-162).", 'api.connect_player_event("my_mod", p, "jumped", _on_jump)'),
 ("connect_net_signal", "mod: String, name: String, callable: Callable", "bool", "net.game_signals", "", "Observes a session/lobby/party signal of the game's Net autoload: lobby_changed, lobby_tod_changed, session_ended, match_started, peer_left_match, peer_match_ready, late_joiner, party_joined, party_became_room, party_ball, party_stage, party_look. Input/snapshot streams, chat and every @rpc are refused.", 'api.connect_net_signal("my_mod", "match_started", _on_match)'),
 ("connect_ball_struck", "mod: String, ball: Node, callable: Callable, flags: int", "bool", "ball.struck", "", "1.0 form of `connect_ball_event(..., \"struck\")` with connect flags.", 'api.connect_ball_struck("my_mod", b, cb, 0)'),
 ("connect_player_touched", "mod: String, player: Node, callable: Callable", "bool", "player.touched", "", "1.0 form for `touched(kind)` (after the rally judged it).", 'api.connect_player_touched("my_mod", p, cb)'),
])
sec("Scene, players, rally (game internals by meaning)", "", [
 ("get_current_scene", "mod: String", "Node", "scene.current", "", "Running scene (menu, match, practice court).", 'api.get_current_scene("my_mod")'),
 ("find_balls", "mod: String", "Array[Node]", "scene.balls", "", "Every node whose script is class Ball.", 'api.find_balls("my_mod")'),
 ("find_players", "mod: String", "Array[Node]", "scene.players", "", "Every Player.", 'api.find_players("my_mod")'),
 ("is_ball", "mod: String, o: Object", "bool", "scene.balls", "", "Is this object a Ball.", 'api.is_ball("my_mod", body)'),
 ("get_local_player", "mod: String", "Node", "scene.players", "", "The local player (`player` on the current scene).", 'api.get_local_player("my_mod")'),
 ("get_rally_for", "mod: String, player: Node", "Object", "scene.rally", "", "The Rally judging this player (main.gd `_rally`).", 'api.get_rally_for("my_mod", p)'),
 ("stage_get_nodes", "mod: String", "Dictionary {stage, court, court_floor, net, grass}", "scene.stage", "", "Stage/court nodes of the current scene (null entries when missing). Read-only handles.", 'api.stage_get_nodes("my_mod").court'),
 ("player_get_state", "mod: String, player: Node", "Dictionary {team_side, serving, contact_state, aim_point, hold_power, grounded, position}", "player.input_state", "", "What the player is doing (player.gd fields). Read-only.", 'api.player_get_state("my_mod", p).serving'),
 ("player_feel_get", "mod: String, prop: String", "Variant", "player.feel", "", "A PlayerFeel field (player_feel.tres). Writes go through `goofy_set_override`.", 'api.player_feel_get("my_mod", "hit_range")'),
 ("get_net_state", "mod: String", "Dictionary {found, active, is_host, lobby_id, steam, party_lan, custom_lobby}", "net.state", "", "Online session state from the Net autoload.", 'api.get_net_state("my_mod").is_host'),
])
sec("Lobby data (sync)", "", [
 ("lobby_data_get", "mod: String, key: String", "String", "net.lobby_data", "", "Steam lobby data value; only `jvm_*` keys.", 'api.lobby_data_get("my_mod", "jvm_rules")'),
 ("lobby_data_set", "mod: String, key: String, value: String", "bool", "net.lobby_data", "", "Host only: writes a `jvm_*` lobby key every member can read.", 'api.lobby_data_set("my_mod", "jvm_rules", "3|1")'),
 ("lobby_data_clear", "mod: String, key: String", "bool", "net.lobby_data", "undo: allowed while closed", "Host only: clears the key.", 'api.lobby_data_clear("my_mod", "jvm_rules")'),
])
sec("Ball", "", [
 ("ball_physics_open", "mod: String, path: String", "String (error or \"\")", "ball.physics", "", "Opens the shared BallPhysics resource (`\"\"` = the game's PHYS) for this mod.", 'api.ball_physics_open("my_mod", "")'),
 ("ball_physics_close", "mod: String", "void", "ball.physics", "", "Forgets this mod's handle (does not restore values).", 'api.ball_physics_close("my_mod")'),
 ("ball_physics_get", "mod: String, prop: String", "Variant", "ball.physics", "", "Reads a BallPhysics field.", 'api.ball_physics_get("my_mod", "radius")'),
 ("ball_physics_set", "mod: String, prop: String, value: Variant", "bool", "ball.physics", "", "Writes a BallPhysics field (unknown fields refused). Affects every ball.", 'api.ball_physics_set("my_mod", "radius", 0.4)'),
 ("ball_physics_restore", "mod: String, prop: String, value: Variant", "bool", "ball.physics", "undo: allowed while closed", "Writes back a saved value.", 'api.ball_physics_restore("my_mod", "radius", r0)'),
 ("ball_get_model", "mod: String, ball: Node", "Node", "ball.model", "", "The ball's dressed visual model.", 'api.ball_get_model("my_mod", b)'),
 ("ball_get_flight", "mod: String, ball: Node", "Dictionary {vel, curve, dive_g, strike_seq, hit_by}", "ball.flight", "", "Flight state.", 'api.ball_get_flight("my_mod", b).vel'),
 ("ball_set_flight", "mod: String, ball: Node, flight: Dictionary", "bool", "ball.flight", "", "Writes vel / curve / dive_g.", 'api.ball_set_flight("my_mod", b, {"curve": 0.5})'),
 ("node_get_scale", "mod: String, cap: String, node: Node", "Vector3", "ball.scale / player.scale", "", "Scale of a ball (`ball.scale`) or player (`player.scale`).", 'api.node_get_scale("my_mod", "ball.scale", b)'),
 ("node_set_scale", "mod: String, cap: String, node: Node, scale: Vector3", "bool", "ball.scale / player.scale", "", "Sets it.", 'api.node_set_scale("my_mod", "player.scale", p, Vector3.ONE * 2)'),
 ("node_restore_scale", "mod: String, cap: String, node: Node, scale: Vector3", "bool", "ball.scale / player.scale", "undo: allowed while closed", "Restores a saved scale.", 'api.node_restore_scale("my_mod", "player.scale", p, s0)'),
])
sec("Rules and the Modifiers panel", "", [
 ("rally_touch_state", "mod: String, rally: Object", "Dictionary {touch_count, touch_side, last_toucher, practice}", "rules.touch_count", "", "Rally touch bookkeeping.", 'api.rally_touch_state("my_mod", r)'),
 ("rally_set_touch_count", "mod: String, rally: Object, count: int", "bool", "rules.touch_count", "", "Rewrites the count (host-enforced).", 'api.rally_set_touch_count("my_mod", r, 0)'),
 ("rally_clear_last_toucher", "mod: String, rally: Object", "bool", "rules.double_hit", "", "Allows the same player to touch again.", 'api.rally_clear_last_toucher("my_mod", r)'),
 ("modifiers_panel_find", "mod: String", "Node", "rules.modifiers_panel", "allowed while closed (to hide rows)", "The custom-lobby Modifiers panel. Uses the menu's own `_modifiers` pointer, then the cached panel; tree walk only as fallback (menu perf).", 'api.modifiers_panel_find("my_mod")'),
 ("modifiers_panel_rows", "mod: String, panel: Node", "Array or null", "rules.modifiers_panel", "allowed while closed", "This mod's rows (null = never added).", 'api.modifiers_panel_rows("my_mod", panel)'),
 ("modifiers_panel_add_rows", "mod: String, panel: Node, defs: Array, on_changed: Callable", "Dictionary", "rules.modifiers_panel", "", "Adds rows built with the panel's own `_rule` / `_entry_row`.", 'api.modifiers_panel_add_rows("my_mod", panel, defs, _on_changed)'),
 ("modifiers_panel_show_rows", "mod: String, rows: Array, show: bool", "void", "rules.modifiers_panel", "allowed while closed", "Shows/hides this mod's rows.", 'api.modifiers_panel_show_rows("my_mod", rows, false)'),
 ("modifiers_panel_set_values", "mod: String, panel: Node, values: Dictionary", "void", "rules.modifiers_panel", "", "Sets row values quietly.", 'api.modifiers_panel_set_values("my_mod", panel, {"max_hits": 3})'),
 ("goofy_rules_get", "mod: String", "Dictionary {max_hits, no_double_hit}", "goofy.query", "", "Goofy's rules.", 'api.goofy_rules_get("my_mod")'),
 ("goofy_rules_set", "mod: String, max_hits: int, no_double_hit: bool", "bool", "goofy.rules", "", "Sets Goofy's rules (host enforces; shown in the Modifiers panel; lobby key `jvm_rules` syncs it in LAN/private/friends).", 'api.goofy_rules_set("my_mod", 4, true)'),
])
sec("Engine, overrides, generic escape hatch", "", [
 ("time_scale_get", "", "float", "-", "", "Engine.time_scale.", "api.time_scale_get()"),
 ("time_scale_set", "mod: String, value: float", "bool", "engine.time_scale", "", "Sets Engine.time_scale on this PC (in a synced match Goofy applies the host's value; see Decisions).", 'api.time_scale_set("my_mod", 0.5)'),
 ("time_scale_restore", "mod: String, value: float", "bool", "engine.time_scale", "undo: allowed while closed", "Restores it.", 'api.time_scale_restore("my_mod", 1.0)'),
 ("overrides_set_rules", "mod: String, rules: Array", "void", "engine.overrides", "", "This mod's override rules ({target, set}).", 'api.overrides_set_rules("my_mod", rules)'),
 ("overrides_apply", "mod: String", "int (nodes changed)", "engine.overrides", "", "Applies them (reversible, per mod).", 'api.overrides_apply("my_mod")'),
 ("overrides_revert", "mod: String", "void", "engine.overrides", "undo: allowed while closed", "Reverts every override of this mod.", 'api.overrides_revert("my_mod")'),
 ("game_get", "mod: String, object: Object, prop: String", "Variant", "game.generic", "", "Escape hatch (logged): read a property.", 'api.game_get("my_mod", n, "x")'),
 ("game_set", "mod: String, object: Object, prop: String, value: Variant", "bool", "game.generic", "", "Escape hatch: write a property.", 'api.game_set("my_mod", n, "x", 5)'),
 ("game_call", "mod: String, object: Object, method: String, args: Array", "Variant", "game.generic", "", "Escape hatch: call a method. @rpc methods, rpc, rpc_id, rpc_config, set_multiplayer_authority refused.", 'api.game_call("my_mod", n, "ok", [])'),
 ("game_connect", "mod: String, object: Object, signal: String, callable: Callable", "bool", "game.generic", "", "Escape hatch: connect a signal.", 'api.game_connect("my_mod", n, "changed", cb)'),
])
sec("Packs, textures, stage / ball picks", "Translation to the unified loader's texture subsystem.", [
 ("content_status", "mod: String", "Dictionary {sharing, repo, auto_download, offer, peers, catalog, downloads, downloaded, rejected, offers_sent, offers_received, shown_ball, shown_stage, cached}", "net.content_offer", "", "Cosmetic sharing state: what this player advertises, what every modded peer wears (per item: have / downloading / installed / hash mismatch / ...), the assets-repo catalog and downloads (docs/CONTENT-SHARING.md). Readable in any mode; sharing itself runs only in LAN / private / friends.", 'api.content_status("my_mod").peers'),
 ("content_fetch", "mod: String, id: String", "bool", "net.content_download", "", "Queue one assets-repo item now (retry after a failed download). Same checks as the automatic path; only in a synced session with the manifest read.", 'api.content_fetch("my_mod", "1011")'),
 ("packs_list", "mod: String, type: String", "Array[Dictionary]", "packs.query", "", "Picker packs of a type (`stage`, `ball`).", 'api.packs_list("my_mod", "stage")'),
 ("pack_info", "mod: String, id: String", "Dictionary", "packs.query", "", "One pack's parsed skin.json view.", 'api.pack_info("my_mod", "1008")'),
 ("packs_rescan", "mod: String", "bool", "packs.rescan", "", "Re-reads pack folders (cached images reused), re-applies picks.", 'api.packs_rescan("my_mod")'),
 ("stage_get_picked", "mod: String", "String", "packs.query", "", "Picked custom court id (\"\" = stock).", 'api.stage_get_picked("my_mod")'),
 ("stage_set_picked", "mod: String, id: String", "bool", "stage.pick", "", "Picks a custom court (saved like the picker does). `\"\"` = stock.", 'api.stage_set_picked("my_mod", "1008")'),
 ("ball_get_picked", "mod: String", "String", "packs.query", "", "Picked custom ball id.", 'api.ball_get_picked("my_mod")'),
 ("ball_set_picked", "mod: String, id: String", "bool", "ball.pick", "", "Picks a custom ball.", 'api.ball_set_picked("my_mod", "1009")'),
 ("cosmetics_apply", "mod: String", "bool", "stage.pick", "", "Applies picked court/ball cosmetics now.", 'api.cosmetics_apply("my_mod")'),
 ("textures_status", "mod: String", "Dictionary", "packs.query", "", "Texture subsystem status.", 'api.textures_status("my_mod")'),
 ("textures_sync_status", "mod: String", "Dictionary", "packs.query", "", "Texture sync (modded peers) status.", 'api.textures_sync_status("my_mod")'),
 ("textures_cache_info", "mod: String", "Dictionary {count, disk_loads, ids, swapped, overrides, spawned, mips}", "packs.query", "", "Process-lifetime texture cache (pack-persist / perf) and mipmap stats (ground moire fix).", 'api.textures_cache_info("my_mod").count'),
 ("texture_get_cached", "mod: String, path: String", "Texture2D or null", "packs.query", "", "A cached pack texture by full path.", 'api.texture_get_cached("my_mod", p)'),
])
sec("Courts: environment and sky", "", [
 ("court_env_info", "mod: String, stage_id: String", "Dictionary {found, has_environment, has_sky, environment, sky, court, ...}", "court.query", "", "A stage pack's environment{} / sky{} / court{} blocks.", 'api.court_env_info("my_mod", "10E1")'),
 ("court_env_status", "mod: String", "Dictionary", "court.query", "", "Environment GLB injection status (clearance verdicts, spawned).", 'api.court_env_status("my_mod")'),
 ("court_sky_status", "mod: String", "Dictionary", "court.query", "", "Distant sky status.", 'api.court_sky_status("my_mod")'),
 ("court_env_load", "mod: String, stage_id: String", "bool", "court.env", "also needs stage.pick", "Picks the court and applies it now: floor/net look + environment GLB (clearance-checked) + sky.", 'api.court_env_load("my_mod", "10E1")'),
])

sec("Native audio (1.4)", "Lazy access to the game's own sound effects and ambience tracks through the unified loader. Enumeration is a cheap index of `res://` paths (`res://game/sfx`, `res://game/ost`); a stream is loaded and a player/host node is created only on the first play request, then freed when unused. Playback is routed through the game's existing Master / sfx / music / ambient / crowd / announcer buses so the user's volume settings apply. Stage packs may declare `audio: { ambience, bus, volume }` in `skin.json`; no shipped pack uses it yet.", [
 ("audio_list", "mod: String", "Array[Dictionary] {id, path, category, bus}", "audio.query", "", "Native sounds as a path index only. Does not load streams or create players/nodes.", 'api.audio_list("my_mod").size()'),
 ("audio_buses", "mod: String", "PackedStringArray", "audio.query", "", "Live AudioServer bus names (Master, sfx, music, ambient, ...). Cheap; does not start playback.", 'api.audio_buses("my_mod")'),
 ("audio_info", "mod: String, id: String", "Dictionary {found, id, path, category, bus}", "audio.query", "", "Resolve a native sound by short id, alias (`beach`) or `res://` path. Builds the path index if needed; does not load the stream.", 'api.audio_info("my_mod", "beach")'),
 ("audio_status", "mod: String", "Dictionary {started, index_built, index_count, cached_streams, players, nodes, idle, ...}", "audio.query", "", "Idle means started=false and no players, host node or cached streams. Listing sounds does not leave idle.", 'api.audio_status("my_mod").idle'),
 ("audio_play", "mod: String, id: String, opts: Dictionary = {}", "int (handle, 0 = fail)", "audio.play", "", "Play a native one-shot. opts: volume (linear), bus, position (Vector3, optional 3D). Creates the host node and loads the stream on first use.", 'api.audio_play("my_mod", "announcer/ace")'),
 ("audio_play_loop", "mod: String, id: String, opts: Dictionary = {}", "int (handle, 0 = fail)", "audio.play", "", "Play a native sound looped (ambience). Same opts as audio_play. The beach ambience id is `beach` / `ambiance/717146__ghosttropic__short-sand-beach-waves-3`.", 'api.audio_play_loop("my_mod", "beach", {"bus": "ambient"})'),
 ("audio_stop", "mod: String, handle_or_id: Variant", "bool", "audio.play", "", "Stop a handle (int) or every voice of a sound id (String). Frees the stream when unused; frees the host node when nothing is playing.", 'api.audio_stop("my_mod", h)'),
 ("audio_stop_all", "mod: String", "bool", "audio.play", "", "Stop every voice started through this API and return to the idle (zero-cost) state.", 'api.audio_stop_all("my_mod")'),
 ("audio_set_volume", "mod: String, handle: int, volume: float", "bool", "audio.play", "", "Per-voice linear gain (the bus still follows the game's Master/sfx/music/ambient volume).", 'api.audio_set_volume("my_mod", h, 0.5)'),
 ("audio_set_bus", "mod: String, handle: int, bus: String", "bool", "audio.play", "", "Move a voice to a game bus (ambient, sfx, music, crowd, announcer, Master, feedback). Unknown names fall back.", 'api.audio_set_bus("my_mod", h, "ambient")'),
 ("audio_apply_pack", "mod: String, stage_id: String", "int (handle, 0 = none)", "audio.play", "", "Apply a stage pack's audio{} block (looped ambience). No-op when the pack has no audio{}.", 'api.audio_apply_pack("my_mod", "10F8")'),
])

sec("Accessories and player models", "", [
 ("accessories_status", "mod: String", "Dictionary {injected, catalog_added, catalog_total, entries, worn, unplaced}", "accessories.query", "", "Locker accessories from packs (Head / Face / Extra).", 'api.accessories_status("my_mod")'),
 ("accessories_list", "mod: String, category: String", "Array[Dictionary] {id, category, part, pack}", "accessories.query", "", "Entries of one category (`head`, `face`, `extra`; \"\" = all).", 'api.accessories_list("my_mod", "head")'),
 ("models_list", "mod: String", "Array", "models.query", "", "Custom player models.", 'api.models_list("my_mod")'),
 ("model_get_saved", "mod: String", "String", "models.query", "", "Saved model id.", 'api.model_get_saved("my_mod")'),
 ("model_get_shown", "mod: String", "String", "models.query", "", "Model shown now (preview or saved).", 'api.model_get_shown("my_mod")'),
 ("model_is_previewing", "mod: String", "bool", "models.query", "", "Locker preview active.", 'api.model_is_previewing("my_mod")'),
 ("model_settings_get", "mod: String, id: String", "Dictionary", "models.query", "", "Per-model settings (colour, options).", 'api.model_settings_get("my_mod", id)'),
 ("models_status", "mod: String", "Dictionary", "models.query", "", "Model subsystem status.", 'api.models_status("my_mod")'),
 ("models_rig_report", "mod: String", "Variant", "models.query", "", "Rig graft report (debug).", 'api.models_rig_report("my_mod")'),
 ("model_preview", "mod: String, id: String", "bool", "player.model", "", "Previews a model (not saved).", 'api.model_preview("my_mod", id)'),
 ("model_apply", "mod: String, id: String", "bool", "player.model", "", "Applies + saves a model.", 'api.model_apply("my_mod", id)'),
 ("model_end_preview", "mod: String", "bool", "player.model", "undo: allowed while closed", "Ends a preview.", 'api.model_end_preview("my_mod")'),
 ("model_set_color", "mod: String, id: String, color: Color", "bool", "player.model", "", "Sets a model's tint (saved).", 'api.model_set_color("my_mod", id, Color.RED)'),
 ("model_reset_color", "mod: String, id: String", "bool", "player.model", "undo: allowed while closed", "Resets the tint.", 'api.model_reset_color("my_mod", id)'),
 ("model_set_option", "mod: String, id: String, key: String, value: Variant", "bool", "player.model", "", "Sets a model option (hands, target...).", 'api.model_set_option("my_mod", id, "hands", 1)'),
])
sec("Unified loader", "", [
 ("loader_status", "mod: String", "Dictionary", "loader.query", "", "Unified loader status.", 'api.loader_status("my_mod")'),
 ("loader_bridge_status", "mod: String", "Dictionary", "loader.query", "", "Goofy bridge state (map, applied pack, goofy version).", 'api.loader_bridge_status("my_mod")'),
 ("loader_is_map", "mod: String, stage_id: String", "bool", "loader.query", "", "Is this stage a loader map.", 'api.loader_is_map("my_mod", "1022")'),
 ("loader_active_map", "mod: String", "String", "loader.query", "", "Current loader map (\"\" = stock).", 'api.loader_active_map("my_mod")'),
 ("loader_map_goofy_block", "mod: String, stage_id: String", "Dictionary", "loader.query", "", "A map's goofy{} block.", 'api.loader_map_goofy_block("my_mod", "1022")'),
 ("loader_refresh", "mod: String", "bool", "stage.pick", "", "Re-evaluates the current map (re-sends goofy block).", 'api.loader_refresh("my_mod")'),
])
sec("Goofy ball (all 156 params)", "Translation to the Goofy API (docs/GOOFY-API.md). The 156-param ball-library schema (BallPhysics x124, PlayerFeel x27, Goofy flight extras x5) is also compiled into core (src/gate/goofy_params.inc), so discovery works without Goofy. Mod layers have owner `mod:<name>`; precedence stock < user config (100) < court pack (200) < mods (default 500, 0..899) < FIXED.", [
 ("goofy_available", "mod: String", "bool", "goofy.query", "", "Goofy API 1.0 installed.", 'api.goofy_available("my_mod")'),
 ("goofy_api_version", "mod: String", "String", "goofy.query", "", "\"1.2\" (\"1.0\" for older Goofy builds) or \"\".", 'api.goofy_api_version("my_mod")'),
 ("goofy_bridge_info", "mod: String", "Dictionary {id, version, enabled} ({} = absent)", "goofy.packs", "", "The goofy library as the loader bridge sees it: instance id (changes when it is replaced), its API version and whether it is enabled. Declaration-checked only.", 'api.goofy_bridge_info("my_mod").get("version", "")'),
 ("goofy_loader_map_set", "mod: String, map: String", "bool", "goofy.packs", "Goofy applies map layers live only where its physics tier allows (offline / LAN / private).", "Tells goofy which loader map is in play (\"\" = none). Used by the unified loader.", 'api.goofy_loader_map_set("my_mod", "beach_day")'),
 ("goofy_pack_apply", "mod: String, map: String, params: Dictionary, version: int", "Dictionary {ok, applied, ignored, errors, active}", "goofy.packs", "As above.", "Hands a map's `goofy{}` block to goofy's pack layer (Goofy API `apply_pack_params`).", 'api.goofy_pack_apply("my_mod", "beach_day", {"ball.gravity": {"mul": 0.9}}, 1)'),
 ("goofy_pack_clear", "mod: String, map: String", "bool", "goofy.packs", "", "Removes that map's pack layer (the map itself stays).", 'api.goofy_pack_clear("my_mod", "beach_day")'),
 ("goofy_schema", "mod: String", "Array[Dictionary] {id, section, target, prop, default, min, max, unit, apply, sync}", "goofy.query", "", "Every param of both libraries (from Goofy when installed, else the core table); 1.2 entries also carry `library` (ball / player), `type` (float / int / color) and `is_color`.", 'api.goofy_schema("my_mod").size()  # 351 = 156 ball + 195 player'),
 ("goofy_param_info", "mod: String, id: String", "Dictionary", "goofy.query", "", "One param from the core table.", 'api.goofy_param_info("my_mod", "ball.gravity")'),
 ("goofy_param_get", "mod: String, id: String", "Variant", "goofy.query", "", "Effective value now (Goofy), or the live resource value without Goofy.", 'api.goofy_param_get("my_mod", "ball.gravity")'),
 ("goofy_params", "mod: String", "Dictionary", "goofy.query", "", "Params differing from stock.", 'api.goofy_params("my_mod")'),
 ("goofy_stock", "mod: String, id: String", "Variant", "goofy.query", "", "The game's stock value.", 'api.goofy_stock("my_mod", "ball.gravity")'),
 ("goofy_layers", "mod: String", "Array[Dictionary]", "goofy.query", "", "Layers in precedence order (debug).", 'api.goofy_layers("my_mod")'),
 ("goofy_ball_mode_get", "mod: String", "String", "goofy.query", "", "\"normal\" / \"fixed\".", 'api.goofy_ball_mode_get("my_mod")'),
 ("goofy_loader_map", "mod: String", "String", "goofy.query", "", "Goofy's view of the loader map.", 'api.goofy_loader_map("my_mod")'),
 ("goofy_physics_allowed", "mod: String", "bool", "goofy.query", "", "Goofy values live now: offline modes (local layers) or LAN / private / friends lobbies (own values in the lobby, the host's gameplay values during a match, see `goofy_sync_status`). Never in public lobbies.", 'api.goofy_physics_allowed("my_mod")'),
 ("goofy_status", "mod: String", "Dictionary", "goofy.query", "", "Goofy library status.", 'api.goofy_status("my_mod")'),
 ("goofy_set_override", "mod: String, params: Dictionary, priority: int = 500, scope: String = \"session\"", "Dictionary {ok, applied, ignored, errors, active}", "goofy.ball", "", "Replaces this mod's BALL layer (ball-library ids; player-library ids are refused into `errors`, use `goofy_player_set_override`). Values: number, [r,g,b,a] or {\"mul\": f}. Priority clamped to 0..899, scope `session` or `stage`. Kept while closed only inside Goofy; the API refuses new writes outside offline modes.", 'api.goofy_set_override("my_mod", {"ball.gravity": {"mul": 0.5}})'),
 ("goofy_clear_override", "mod: String", "bool", "goofy.ball", "undo: allowed while closed", "Removes this mod's layer.", 'api.goofy_clear_override("my_mod")'),
 ("goofy_ball_mode_set", "mod: String, mode: String", "bool", "goofy.ball_mode", "", "Ball override NORMAL/FIXED (persisted by Goofy).", 'api.goofy_ball_mode_set("my_mod", "fixed")'),
])
sec("Player consts (1.2)", "The 34 gameplay values hard-coded in game/player/player.gd (docs/PLAYER-CONSTS.md). **HARD REQUIREMENT: every player needs player.consts (same game build). If anyone in the match lacks it, the host turns the edited values off for the whole match and everyone plays stock; the host gets a notice naming who.** Values are match-wide. Edit in the lobby / menu only: at match start the set is frozen and the host sends it to every player; any write during a match (API, Goofy UI, sync packet) is refused with a logged warning and an `error`. Unlocked again at match end (exact stock restored).", [
 ("player_consts_get", "mod: String", "Dictionary {available, reason, manifest_id, locked, applied, saved, in_play, stock, notice, rejected, warnings}", "player.consts", "read: allowed everywhere", "State: whether the boot rewrite is active, the saved lobby set, the values live now, the last host notice.", 'api.player_consts_get("my_mod")'),
 ("player_consts_set", "mod: String, name: String, value: float", "Dictionary {ok, error}", "player.consts", "lobby only", "Saves one value (clamped to its range) for the next match. During a match: `ok=false` + error, value unchanged.", 'api.player_consts_set("my_mod", "BLOCK_TURN", 0.5)'),
 ("player_consts_reset", "mod: String, name: String", "Dictionary {ok, error}", "player.consts", "lobby only", "Back to the stock value (`\"*\"` = all 34). Refused during a match like `player_consts_set`.", 'api.player_consts_reset("my_mod", "*")'),
 ("player_consts_locked", "mod: String", "bool", "player.consts", "read", "True while a match runs (edits refused, UI greyed).", 'api.player_consts_locked("my_mod")'),
])
sec("Goofy ball + player libraries, host sync (1.2)", "Goofy API 1.2 (docs/GOOFY-BALL.md, docs/GOOFY-PLAYER.md, docs/GOOFY-SYNC.md). Ball and player indices are positions in `goofy_balls()` / `goofy_players()` (= `find_balls` / `find_players` order) at the time of the call. Live state reads are read-only and safe everywhere; writes need an offline mode. In LAN / private / friends lobbies the HOST's effective params are used by every modded player (clients ignore their own layers until they leave); public lobbies never sync and never apply Goofy physics.", [
 ("goofy_ball_schema", "mod: String", "Array[Dictionary]", "goofy.query", "", "The 156 ball-library params: physics, bounce / collision, hitbox, net geometry, serve, shot shaping, speed limits, spin / curve, visuals (outline / trail / contest), hit range, player physics, drag.", 'api.goofy_ball_schema("my_mod").size()  # 156'),
 ("goofy_player_schema", "mod: String", "Array[Dictionary]", "goofy.query", "", "The 195 player-library params: movement, jump, sprint / stamina, dive / fall, block, serve, animation, visuals, contact timing (PlayerFeel) and `player.body_scale`.", 'api.goofy_player_schema("my_mod").size()  # 195'),
 ("goofy_balls", "mod: String", "Array[Dictionary]", "goofy.query", "", "`goofy_ball_state` of every ball.", 'api.goofy_balls("my_mod")'),
 ("goofy_ball_state", "mod: String, index: int", "Dictionary {index, position, velocity, speed, curve, dive_g, strike_seq, frozen, serve_held, net_puppet, remote_driven, float_serve, hitstop_left, hit_by, radius, scale, airborne}", "goofy.query", "", "One ball's live state; `hit_by` = player index or -1. {} for a bad index.", 'api.goofy_ball_state("my_mod", 0).speed'),
 ("goofy_ball_set_velocity", "mod: String, index: int, velocity: Vector3", "bool", "goofy.ball_live", "", "Sets the ball's velocity now (finite only, length capped at 200). Unsynced, so offline only.", 'api.goofy_ball_set_velocity("my_mod", 0, Vector3(0, 8, -4))'),
 ("goofy_ball_set_curve", "mod: String, index: int, curve: float", "bool", "goofy.ball_live", "", "Sets the ball's curve (spin) now, clamped to -100..100. Offline only.", 'api.goofy_ball_set_curve("my_mod", 0, 2.5)'),
 ("goofy_ball_impulse", "mod: String, index: int, delta_v: Vector3", "bool", "goofy.ball_live", "", "Adds `delta_v` to the ball's velocity (capped at 200). Offline only.", 'api.goofy_ball_impulse("my_mod", 0, Vector3.UP * 3)'),
 ("goofy_players", "mod: String", "Array[Dictionary]", "goofy.query", "", "`goofy_player_state` of every player.", 'api.goofy_players("my_mod")'),
 ("goofy_player_state", "mod: String, index: int", "Dictionary {index, local, team_side, serving, contact_state, aim_point, hold_power, grounded, position, velocity, speed, sprinting, stamina, is_bot, dive_cooldown, scale, body_scale, anim {clip, pos, speed}}", "goofy.query", "", "One player's live state incl. the animation state of its rig. {} for a bad index.", 'api.goofy_player_state("my_mod", 0).anim'),
 ("goofy_player_cosmetics", "mod: String, index: int", "Dictionary {index, local, has_character, display_name, number, skin_tone, hair_style, hair_color, accessories {slot: id}, look, model}", "goofy.query", "", "Read-only cosmetics state from the player's character config (+ the shown custom model for the local player).", 'api.goofy_player_cosmetics("my_mod", 0).accessories'),
 ("goofy_player_set_override", "mod: String, params: Dictionary, priority: int = 500, scope: String = \"session\"", "Dictionary {ok, applied, ignored, errors, active}", "goofy.player", "", "Replaces this mod's PLAYER layer (owner `mod:<name>:player`; player-library ids only, ball ids are refused into `errors`). Same values / priority / scope rules as `goofy_set_override`.", 'api.goofy_player_set_override("my_mod", {"player.move_speed": {"mul": 1.3}, "player.body_scale": 1.5})'),
 ("goofy_player_clear_override", "mod: String", "bool", "goofy.player", "undo: allowed while closed", "Removes this mod's player layer.", 'api.goofy_player_clear_override("my_mod")'),
 ("goofy_sync_status", "mod: String", "Dictionary {role, authoritative, live, host_left, host_values, time_scale, time_scale_synced, peers, sent, received, rejected, seq, last_error, version}", "goofy.query", "", "Host sync: role off / offline / host / client, whose gameplay values are live (local / host / none; a client follows the host from match start to match end), the synced time scale and packet counters (payload version 2).", 'api.goofy_sync_status("my_mod").role'),
])

sec("Gameplay settings registry and game rules (1.3)", "The translation layer for settings that change play (docs/GAMEPLAY-SETTINGS.md, docs/GOOFY-RULES.md). A mod registers its settings once during its config setup; the core persists them per mod (`mods/api/state/gameplay/<mod>.json`, versioned), builds their controls in the game's Settings panel (`mods` tab) and answers every read through `gameplay_get`: stock while gameplay is closed (public lobbies / quick match), the host's value for clients in LAN / private / friends lobbies (stock until the host's values arrive), else the local value. Values are always validated and clamped. Visual-only settings are refused: cosmetics stay per-player.", [
 ("gameplay_register", "mod: String, defs: Array[Dictionary], options: Dictionary {version, migrate, on_change}", "Dictionary {ok, registered, errors, version, migrated_from, loaded, role}", "gameplay.settings", "allowed while gameplay is closed (registering changes nothing in play)", "Registers settings. Each def: `id` (`<mod>.<name>`, lowercase), `type` (bool / int / float / enum / vec), `default`, `min` / `max` / `step` (numbers; vec per component), `options` (enum), `label`, `tooltip`, `category`, `order`, `sync` (must be `gameplay`), optional `on_change(id, value, source)` and `on_read(id, value, source) -> value` callables. Bad defs are listed in `errors`, the rest register. Stored values load through `options.migrate(from_version, values) -> values` when the stored schema is older than `options.version` (0 = never saved). `options.on_change` is a mod-wide change hook.", 'api.gameplay_register("my_mod", [{"id": "my_mod.gravity", "type": "float", "default": 1.0, "min": 0.1, "max": 3.0, "sync": "gameplay"}], {"version": 1})'),
 ("gameplay_register_setting", "mod: String, def: Dictionary", "Dictionary", "gameplay.settings", "allowed while closed", "One setting; same as `gameplay_register(mod, [def])`.", 'api.gameplay_register_setting("my_mod", def)'),
 ("gameplay_unregister", "mod: String", "int", "gameplay.settings", "allowed while closed", "Removes the mod's settings (values stay saved). Returns how many were removed.", 'api.gameplay_unregister("my_mod")'),
 ("gameplay_get", "mod: String, id: String", "Variant", "gameplay.settings", "allowed while closed (returns stock then)", "The effective value of any registered setting through the read hook: role, host override, clamp, then `on_read`. Enums are option names, vecs are Vector2/3/4. null when unknown.", 'var g: float = api.gameplay_get("my_mod", "my_mod.gravity")'),
 ("gameplay_get_all", "mod: String, owner: String = \"\"", "Dictionary {id: value}", "gameplay.settings", "allowed while closed", "Effective values of every setting, or only `owner`'s.", 'api.gameplay_get_all("my_mod", "my_mod")'),
 ("gameplay_set", "mod: String, id: String, value: Variant", "Dictionary {ok, error, value}", "gameplay.settings", "", "Sets the local value of one of the mod's own settings (validated, snapped to step, clamped, saved). Refused for clients in a synced lobby (the host controls them) and for other mods' settings.", 'api.gameplay_set("my_mod", "my_mod.gravity", 0.5)'),
 ("gameplay_reset", "mod: String, id: String", "Dictionary {ok, error, value}", "gameplay.settings", "", "Back to the default (removed from the state file).", 'api.gameplay_reset("my_mod", "my_mod.gravity")'),
 ("gameplay_list", "mod: String", "Array[Dictionary]", "gameplay.settings", "allowed while closed", "Every registered setting with its definition plus `value` (effective), `local`, `source` (default / local / host / stock) and `locked`.", 'for s in api.gameplay_list("my_mod"): print(s.id, s.value)'),
 ("gameplay_info", "mod: String, id: String", "Dictionary", "gameplay.settings", "allowed while closed", "One entry of `gameplay_list`; empty when unknown.", 'api.gameplay_info("my_mod", "my_mod.gravity").source'),
 ("gameplay_role", "mod: String", "String", "gameplay.settings", "allowed while closed", "off (stock: gameplay closed), local (offline or the host) or client (synced lobby, not the host).", 'api.gameplay_role("my_mod")'),
 ("gameplay_snapshot", "mod: String", "Dictionary {ok, version, values, hash, bytes}", "gameplay.settings_sync", "", "Host side of host sync: the effective value of every registered setting (`bytes` = the JVGS text form, `hash` = FNV-1a 64 hex). Host / offline only.", 'var snap = api.gameplay_snapshot("my_sync")'),
 ("gameplay_snapshot_bytes", "mod: String", "PackedByteArray", "gameplay.settings_sync", "", "The snapshot as compact bytes (`JVGS 1` then `id=value` lines, max 32 KiB) for a lobby packet. Empty when not allowed.", 'var b: PackedByteArray = api.gameplay_snapshot_bytes("my_sync")'),
 ("gameplay_apply_snapshot", "mod: String, payload: PackedByteArray | String | Dictionary {version, values}", "Dictionary {ok, error, applied, rejected, unknown}", "gameplay.settings_sync", "", "Client side: replaces the host's values with a received snapshot. Complete replacement (settings the host did not send read as stock); each value validated and clamped to this side's range; unknown ids ignored. Refused as host / offline.", 'api.gameplay_apply_snapshot("my_sync", packet)'),
 ("gameplay_clear_host", "mod: String", "bool", "gameplay.settings_sync", "allowed while closed", "Forgets the host's values (lobby left / host changed).", 'api.gameplay_clear_host("my_sync")'),
 ("gameplay_manifest", "mod: String", "Dictionary {mods: {mod: version}, loader_ok, bytes}", "gameplay.settings", "allowed while closed", "This player's gameplay mod list for the match-wide missing-mod rule: every mod with registered gameplay settings or a gameplay / sync / offline capability, with its version (`options.mod_version`, else the settings schema, else the API version it registered with). Texture / model / loader / core UI libraries are never listed, so the rule never disables them. `loader_ok` = the texture loader (jvm_loader) is running.", 'api.gameplay_manifest("my_sync").mods'),
 ("gameplay_manifest_bytes", "mod: String", "PackedByteArray", "gameplay.settings_sync", "", "The list as bytes for a lobby packet (`JVGM 1`, `loader=0|1`, `mod=version` lines; the host's list also carries `!mod=1` for every mod it disabled for the match; max 8 KiB). Every player sends it to the host; the host sends its own to everyone after `gameplay_match_start`.", 'var b = api.gameplay_manifest_bytes("my_sync")'),
 ("gameplay_set_peer_manifest", "mod: String, player_id: String, payload: PackedByteArray | String, player_name: String = \"\"", "Dictionary {ok, error, player, name, mods, loader_ok, missing, late, locked, disabled}", "gameplay.settings_sync", "", "Host: records a player's list. `missing` = the host's gameplay mods that player lacks (mod -> `missing` / `version x (host y)`); they will be off for EVERYONE from the next `gameplay_match_start`. `late` = joined while a match is locked (counted at the next match start). Logged, `gameplay_mods_status` fired, and the host gets an on-screen notice when that player's texture loader is not running. Host only.", 'api.gameplay_set_peer_manifest("my_sync", str(peer_id), packet, peer_name)'),
 ("gameplay_remove_peer", "mod: String, player_id: String", "bool", "gameplay.settings_sync", "allowed while closed", "Forgets a player's list (they left). A locked match keeps its decision until it ends.", 'api.gameplay_remove_peer("my_sync", str(peer_id))'),
 ("gameplay_match_start", "mod: String", "Dictionary {ok, error, disabled: {mod: [{player, name, why}]}, active, locked}", "gameplay.settings_sync", "", "Host, at match start: decides once for the whole match. A gameplay mod that any known player lacks (or has in another major version) is disabled for everyone, host included: its settings read stock, it is left out of snapshots, the core refuses its gameplay calls, and `gameplay_mod_active_for` says false. The host gets an on-screen notice naming who is missing what. Settings are locked (no edits) until `gameplay_match_end`.", 'api.gameplay_match_start("my_sync")'),
 ("gameplay_match_end", "mod: String", "bool", "gameplay.settings_sync", "allowed while closed", "Host, at match end: unlocks settings and turns every mod back on. Leaving the lobby does the same.", 'api.gameplay_match_end("my_sync")'),
 ("gameplay_set_host_manifest", "mod: String, payload: PackedByteArray | String", "Dictionary {ok, error, disabled, active, missing_here, locked, host_loader_ok}", "gameplay.settings_sync", "", "Client: records the host's list and its match decision. Mods the host disabled (`!` lines), does not have, or has in another major version run stock here too. `missing_here` = disabled mods this player lacks; this player gets an on-screen notice for those.", 'api.gameplay_set_host_manifest("my_sync", packet)'),
 ("gameplay_mod_active_for", "mod: String, mod_id: String, player_id: String = \"\"", "bool", "gameplay.settings", "allowed while closed", "THE shared check for the match-wide missing-mod rule: is gameplay mod `mod_id` on in this match. The answer is the same for every player (`player_id` is accepted for callers that pass one). False while gameplay is closed, when the mod is not installed, when the host disabled it for the match, and on a client until the host's list arrived (fail closed) or when the host lacks it.", 'if api.gameplay_mod_active_for("my_mod", "my_mod"): ...'),
 ("gameplay_match_status", "mod: String", "Dictionary {role, locked, disabled, active, would_disable}", "gameplay.settings", "allowed while closed", "The match decision: `disabled` (mod -> who lacks it), `active` mods, `locked`; on the host before match start also `would_disable` (mod -> names).", 'api.gameplay_match_status("my_mod").disabled'),
 ("gameplay_player_mods", "mod: String", "Dictionary {player_id: {name, mods, loader_ok, missing, late}}", "gameplay.settings", "allowed while closed", "Per-player status: `local` (this player: mods, loader_ok, active, inactive with reasons), every player whose list the host has (missing = the host's mods they lack, late), and `host` (its list and disabled mods) on a client.", 'api.gameplay_player_mods("my_mod")'),
 ("game_rules_info", "mod: String", "Dictionary {touches_per_side, consecutive_per_player, double_hit, solo_side_exempt, over_team_limit, block_counts, practice_exempt, block_classic}", "game.rules", "", "The stock rally rules as the game has them (from the Rules script constants where possible). Read-only.", 'api.game_rules_info("my_mod").touches_per_side  # 3'),
 ("game_rules_state", "mod: String", "Dictionary {count, side, practice, has_last_toucher, last_toucher_id}", "game.rules", "", "The live rally touch state (host / offline). Empty when there is no rally.", 'api.game_rules_state("my_mod").count'),
])
sec("Shots and curve", "", [
 ("shots_list", "mod: String", "PackedStringArray", "shots.query", "", "Configured extra shot ids.", 'api.shots_list("my_mod")'),
 ("shots_armed", "mod: String", "String", "shots.query", "", "Armed shot id.", 'api.shots_armed("my_mod")'),
 ("shots_last", "mod: String, kind: String", "Variant", "shots.query", "", "Last spin / topspin / curve / float / event (null for other kinds).", 'api.shots_last("my_mod", "curve")'),
 ("shots_spin_bar", "mod: String", "Variant", "shots.query", "", "Spin bar state.", 'api.shots_spin_bar("my_mod")'),
 ("shots_range_box", "mod: String", "Variant", "shots.query", "", "Range box.", 'api.shots_range_box("my_mod")'),
 ("shots_counts", "mod: String", "Dictionary {applied, skipped, touches}", "shots.query", "", "Counters.", 'api.shots_counts("my_mod")'),
 ("shots_status", "mod: String", "Dictionary", "shots.query", "", "Shots status.", 'api.shots_status("my_mod")'),
 ("shot_arm", "mod: String, id: String", "bool", "shots.arm", "", "Arms a shot for the next contact.", 'api.shot_arm("my_mod", "high_shot")'),
 ("shots_debug_set", "mod: String, on: bool", "bool", "shots.debug", "turning off allowed while closed", "Debug overlay on/off.", 'api.shots_debug_set("my_mod", true)'),
 ("shots_debug_visible", "mod: String", "bool", "shots.debug", "", "Overlay visible.", 'api.shots_debug_visible("my_mod")'),
 ("shots_debug_text", "mod: String", "String", "shots.debug", "", "Overlay text.", 'api.shots_debug_text("my_mod")'),
])
sec("Library game IO", "How our own libraries (goofy, shots, textures, models, the unified loader, core UI) reach the game script surface: every script property / method / signal, stock node path, autoload, engine singleton and stock resource goes through these, under a capability the library declared. Engine built-ins on objects the library owns stay local. C++ libraries use `jvm::GameIO` (src/common/api_client.hpp), which registers the library and calls these. Game `@rpc` methods are never called. `undo=true` = a revert path (allowed while the class is closed); libraries whose on/off already follows their access class (cosmetic / local UI) are declaration-checked only.", [
 ("io_get", "mod: String, cap: String, obj: Object, prop: StringName", "Variant", "(declared cap)", "", "obj.get(prop) when `cap` is declared by `mod` (reads are declaration-checked).", 'api.io_get("my_lib", "ball.flight", ball, "curve")'),
 ("io_set", "mod: String, cap: String, obj: Object, prop: StringName, value: Variant, undo: bool", "bool", "(declared cap)", "Needs the class open unless `undo`.", "obj.set(prop, value).", 'api.io_set("my_lib", "ball.flight", ball, "curve", 0.0, false)'),
 ("io_call", "mod: String, cap: String, obj: Object, method: StringName, args: Array, undo: bool", "Variant", "(declared cap)", "Needs the class open unless `undo`; game `@rpc` methods are refused.", "obj.callv(method, args).", 'api.io_call("my_lib", "ball.solver", ball, "solve_launch", [from, to, 1, 0.8, false, Vector2()], false)'),
 ("io_connect", "mod: String, cap: String, obj: Object, signal: StringName, callable: Callable, flags: int", "int (Error)", "(declared cap)", "", "obj.connect(signal, callable, flags). Observing is a read; the handler's own writes are checked.", 'api.io_connect("my_lib", "ball.struck", ball, "struck", _on_struck, 0)'),
 ("io_disconnect", "mod: String, cap: String, obj: Object, signal: StringName, callable: Callable", "bool", "(declared cap)", "", "Disconnects when connected.", 'api.io_disconnect("my_lib", "ball.struck", ball, "struck", _on_struck)'),
 ("io_node", "mod: String, cap: String, from: Node, path: String", "Node", "(declared cap)", "", "from.get_node_or_null(path): a stock node path (Court, Stage/Grass, Net ...).", 'api.io_node("my_lib", "stage.court_look", scene, "Court")'),
 ("io_autoload", "mod: String, cap: String, name: String", "Node", "(declared cap)", "", "A game autoload (/root/<name>: GameConfig, Net, Achievements ...).", 'api.io_autoload("my_lib", "ui.notify", "Achievements")'),
 ("io_singleton", "mod: String, cap: String, name: String", "Object", "(declared cap)", "", "An engine singleton the game registers (Steam).", 'api.io_singleton("my_lib", "net.member_data", "Steam")'),
 ("io_resource", "mod: String, cap: String, path: String", "Resource", "(declared cap)", "", "A stock `res://` resource (shared, Godot-cached), e.g. the PlayerFeel tuning or a game script.", 'api.io_resource("my_lib", "player.feel", "res://game/player/player_feel.tres")'),
])
sec("Picker, Mods panel, menu perf", "", [
 ("picker_ball_open", "mod: String", "bool", "stage.picker", "", "Opens the Free Play ball picker.", 'api.picker_ball_open("my_mod")'),
 ("picker_stage_inject", "mod: String", "bool", "stage.picker", "", "Injects custom court tiles into the stage picker now.", 'api.picker_stage_inject("my_mod")'),
 ("mods_panel_open", "mod: String", "bool", "ui.mods_panel", "", "Opens our Mods panel.", 'api.mods_panel_open("my_mod")'),
 ("mods_panel_close", "mod: String", "bool", "ui.mods_panel", "allowed while closed", "Closes it.", 'api.mods_panel_close("my_mod")'),
 ("mods_panel_show_list", "mod: String", "bool", "ui.mods_panel", "", "Shows the list page.", 'api.mods_panel_show_list("my_mod")'),
 ("mods_panel_show_detail", "mod: String, mod_id: String", "bool", "ui.mods_panel", "", "Shows a mod's detail page.", 'api.mods_panel_show_detail("my_mod", "goofy")'),
 ("mods_panel_info", "mod: String", "Dictionary", "ui.mods_panel", "", "Panel state.", 'api.mods_panel_info("my_mod")'),
 ("mods_config_files", "mod: String, mod_id: String", "Variant (Array)", "ui.mods_panel", "", "Config files of a mod category.", 'api.mods_config_files("my_mod", "shots")'),
 ("mods_config_save", "mod: String, path: String, text: String, live: bool", "Variant (result)", "ui.mods_panel", "", "Validates then saves a config file; `live` re-applies.", 'api.mods_config_save("my_mod", p, txt, true)'),
 ("menu_perf_get", "mod: String", "bool", "menu.perf", "", "Menu perf on.", 'api.menu_perf_get("my_mod")'),
 ("menu_perf_set", "mod: String, on: bool", "bool", "menu.perf", "", "Turns the cheaper menu motion on/off for this session (the saved default is `mods/core/00-default.json` `menu_perf`). Same look either way.", 'api.menu_perf_set("my_mod", false)'),
 ("menu_perf_info", "mod: String", "Dictionary", "menu.perf", "", "Counters.", 'api.menu_perf_info("my_mod")'),
 ("menu_perf_reset", "mod: String", "bool", "menu.perf", "", "Resets counters.", 'api.menu_perf_reset("my_mod")'),
])

sec("Developer tools (1.3)", "Frame profiler, debugger and the player.consts checker (docs/DEVTOOLS.md). All local to this PC; nothing here changes the game. The profiler is idle (no per-frame work) until a capture starts; the debugger is off by default.", [
 ("profiler_start", "mod: String, seconds: float = 2.0, tag: String = \"\"", "Dictionary {ok, reason, seconds, tag}", "dev.profiler", "", "Starts a capture of `seconds` (0.1-30) from the next frame: every frame's physics / process / render / rest and every CORE + library hook (texture rescan, scene cosmetics, court env / sky apply, net ticks, log writes, goofy / shots physics ...). Refused while one runs. When it ends the JSON + CSV land in `user://jv_profiler/` and the profiler goes idle again.", 'api.profiler_start("my_mod", 2.0, "snow")'),
 ("profiler_toggle", "mod: String, seconds: float = 0.0, tag: String = \"api\"", "Dictionary {ok, reason, seconds, tag, toggled}", "dev.profiler", "undo: stopping is allowed while the class is closed", "The same switch as the profiler hotkey and the Mods panel `dev tools: frame profiler` entry: idle -> starts a capture (`seconds` <= 0 uses `profiler_seconds`, 2 s by default), capturing -> stops it now and keeps it. `toggled` is \"started\" or \"stopped\".", 'api.profiler_toggle("my_mod")'),
 ("profiler_stop", "mod: String", "Dictionary (capture without frames) or {ok: false}", "dev.profiler", "undo: allowed while the class is closed", "Ends the running capture early and keeps it.", 'api.profiler_stop("my_mod")'),
 ("profiler_status", "mod: String", "Dictionary {capturing, seconds, elapsed, frames, captures, last_json, last_csv, default_seconds, hotkeys, hotkeys_source, dev_build, profiler_key, console_key}", "dev.profiler", "", "Profiler state and the last files written.", 'api.profiler_status("my_mod").capturing'),
 ("profiler_last_capture", "mod: String, frames: bool = false", "Dictionary {meta, summary, stages, worst, hitches, stage_names, frames?}", "dev.profiler", "", "The last capture. `frames` true adds one row per frame (built-in stages + `stages` {name: ms}).", 'api.profiler_last_capture("my_mod", true).frames.size()'),
 ("profiler_summary", "mod: String", "Dictionary {meta, summary, stages}", "dev.profiler", "", "Per-stage mean / p50 / p95 / max / total ms and calls of the last capture.", 'api.profiler_summary("my_mod").stages["textures.rescan"].max_ms'),
 ("profiler_worst_frames", "mod: String, n: int = 5", "Array[Dictionary]", "dev.profiler", "", "The n slowest frames, each with its full breakdown and `top` contributors.", 'api.profiler_worst_frames("my_mod", 3)'),
 ("profiler_hitches", "mod: String", "Dictionary {threshold_ms, list, intervals_ms, interval_mean_ms, interval_median_ms, explained_by, per_stage}", "dev.profiler", "", "Frames above max(2 x median, median + 8 ms), the time between them, and which stages are slower in hitch frames than in normal ones.", 'api.profiler_hitches("my_mod").explained_by'),
 ("debug_set_enabled", "mod: String, on: bool", "bool", "dev.debugger", "undo: turning it off is allowed while the class is closed", "Debugger on/off (off by default; mods/core `devtools.debugger`). Off clears the trace and hides the console.", 'api.debug_set_enabled("my_mod", true)'),
 ("debug_is_enabled", "mod: String", "bool", "dev.debugger", "", "Debugger state.", 'api.debug_is_enabled("my_mod")'),
 ("debug_set_log_level", "mod: String, tag: String, level: int", "bool", "dev.debugger", "undo: level -1 (reset) is allowed while the class is closed", "Log level of one library tag (textures, shots, goofy, models, loader, api, gate, consts, devtools, ... or `*`): 0 debug, 1 info (default), 2 warnings only, 3 off, -1 reset. Shared by every library (Engine meta `jv_log_levels`).", 'api.debug_set_log_level("my_mod", "textures", 2)'),
 ("debug_log_levels", "mod: String", "Dictionary {tag: level}", "dev.debugger", "", "Levels set so far.", 'api.debug_log_levels("my_mod")'),
 ("debug_capabilities", "mod: String", "Dictionary {capabilities, by_status, open_by_status, mode, classes, mods, libraries, api_version}", "dev.debugger", "", "Capability table with impl / mapped / planned and live open flags, access classes, registered mods with counters, libraries with their status and profiler taps.", 'api.debug_capabilities("my_mod").by_status'),
 ("debug_tree", "mod: String, path: String = \"\", depth: int = 1", "Array or Dictionary", "dev.debugger", "", "Our own nodes only: with no path, every /root/JV* node and every JVM_* node we added to the running scene; with a path, that node (it must be ours) and `depth` levels of children.", 'api.debug_tree("my_mod", "/root/JVModGate", 2)'),
 ("debug_node", "mod: String, path: String", "Dictionary {name, path, class, script, visible, processing, children, properties, meta}", "dev.debugger", "", "Properties (as text) and meta keys of one of our nodes; empty for game nodes.", 'api.debug_node("my_mod", "/root/JVModGate/JVDevTools")'),
 ("debug_trace", "mod: String, n: int = 64", "Array[Dictionary] {t_ms, frame, kind, name, detail}", "dev.debugger", "", "Last n trace entries (event bus, refused calls, profiler, consts findings, match start/end). Recorded only while the debugger is on (512 kept).", 'api.debug_trace("my_mod", 20)'),
 ("debug_trace_clear", "mod: String", "bool", "dev.debugger", "undo: allowed while the class is closed", "Empties the trace.", 'api.debug_trace_clear("my_mod")'),
 ("debug_dump", "mod: String", "String (user:// path)", "dev.debugger", "", "Writes capabilities, levels, our node tree, trace, consts status and findings and profiler state to `user://jv_profiler/debug-dump-<time>.json`.", 'api.debug_dump("my_mod")'),
 ("debug_console_toggle", "mod: String", "bool", "dev.debugger", "", "Shows / hides the in-game console (turns the debugger on). Commands: help, status, caps, mods, libs, log, levels, tree, node, trace, prof, last, worst, hitches, consts, audit, check, dump.", 'api.debug_console_toggle("my_mod")'),
 ("consts_live", "mod: String", "Dictionary {available, reason, statics, consts, source}", "player.consts_check", "", "What the player.consts bypass reports through the ONE adapter: its read API when present, else a direct read of the already-loaded player script (never loads it).", 'api.consts_live("my_mod").statics.size()'),
 ("consts_status", "mod: String", "Dictionary {available, reason, source, statics, still_const, bypass_active, match_active, frozen, mode, is_host, synced_lobby, cap_open, start_set_valid, start_set, host_set_valid, host_set, approved, findings, errors, audits, checks, audit_enabled}", "player.consts_check", "", "Checker state. `frozen` is true for the whole match.", 'api.consts_status("my_mod").frozen'),
 ("consts_check_write", "mod: String, name: String, value: Variant", "Dictionary {ok, findings}", "player.consts_check", "", "Judges a write before it happens (the player.consts setter calls this; mods may too). Errors: CONST_NOT_STATIC, UNKNOWN_NAME, NOT_GAMEPLAY, PROTOCOL_NAME, BAD_TYPE, BAD_VALUE, OUT_OF_RANGE, CAP_UNAVAILABLE, NON_HOST_WRITE, MID_MATCH_WRITE (any write once the match started). An accepted value becomes the expected one for the audit.", 'api.consts_check_write("goofy", "SET_DUMP_RISE_TAP", 3.0).ok'),
 ("consts_audit", "mod: String", "Dictionary {ok, findings, bypass_active, source}", "player.consts_check", "", "Checks the live values now (also runs at match start and once a second while the bypass is active): MID_MATCH_CHANGE, HOST_MISMATCH, UNTRACKED_WRITE, NOT_STOCK_WHILE_CLOSED, OUT_OF_RANGE, PROTOCOL_REWRITTEN, CONST_NOT_REWRITTEN, FOLDED_COPY_STALE (warning).", 'api.consts_audit("my_mod").findings'),
 ("consts_findings", "mod: String", "Array[Dictionary] {severity, code, name, detail, source, count, first_ms, last_ms}", "player.consts_check", "", "Every finding so far (deduplicated with counts, 500 kept). Errors are also logged once as `[jv-mods:consts] WARNING ...`.", 'api.consts_findings("my_mod")'),
 ("consts_clear_findings", "mod: String", "bool", "player.consts_check", "undo: allowed while the class is closed", "Forgets the findings.", 'api.consts_clear_findings("my_mod")'),
 ("consts_set_host_values", "mod: String, values: Dictionary", "bool", "player.consts_check", "", "Client side: the host's start-of-match set as received by the sync layer; live values that differ are HOST_MISMATCH errors. On the host (and offline) the start-of-match capture is the host set.", 'api.consts_set_host_values("goofy", host_values)'),
])

def sig(name, params): return f"`{name}({params})`"

out = []
w = out.append
w("# jv-mods CORE API 1.3 — reference")
w("")
w("<!-- GENERATED by tools/gen_core_api_doc.py from its table + src/api/capabilities.def. Do not edit by hand. -->")
w("")
w("CORE (`jvm_core`) is the translation layer every mod hooks into. Mods name a feature (\"pick this court\", \"ball gravity x0.5\", \"tell me when the mode changes\"); CORE knows where the game and our libraries keep it (node paths, script fields, method names - each game reference is cited as `file:line` in `src/api/capabilities.def` and verified by `tools/check_api_refs.py`). When the game or a library changes, CORE is fixed in one place and mods keep working.")
w("")
w("* **Where:** `Engine.get_singleton(\"JVModAPI\")` (GDScript) or `/root/JVModGate/JVModAPI`. C++: `jvm::find_api()` in `src/common/api_client.hpp`.")
w("* **Versioning:** `major.minor.patch`. A mod registers with the version it was written for; same major and minor <= ours is accepted. Minor bumps only add functions / capabilities / events; nothing listed here changes meaning within major 1. Each function lists the API version it appeared in.")
w("* **Every call names the mod** (first argument) and is checked against (1) the capabilities the mod declared in `register_mod` and (2) the gate rule of the capability's access class. Refused calls return a neutral value (false / empty / null) and are logged once per (mod, capability, reason).")
w("* **Undo always works:** calls marked *undo* only need the capability declared, so a mod can revert after its class closed.")
w("* **Fail closed:** if the core leaves, every class reports closed (`access_changed`) before the API goes away.")
w("* **Not installed?** Functions for a subsystem that is not installed return the neutral value; check `libraries_status`.")
w("* **Runtime only:** nothing here edits the game's pck or exe.")
w("")
w("## Access classes and multiplayer safety")
w("")
w("| access | open in | multiplayer |")
w("|---|---|---|")
OPEN = {"read": "every mode once pinned", "local_ui": "every mode once pinned", "cosmetic": "every identified mode (incl. public)", "gameplay": "Free Play, practice, offline solo, menu, LAN, private, friends-only", "sync": "LAN, private, friends-only", "offline": "Free Play, practice, offline solo, menu"}
for a in ["read", "local_ui", "cosmetic", "gameplay", "sync", "offline"]:
    w(f"| `{a}` | {OPEN[a]} | {MP[a]} |")
w("")
w("Gate rules (rules.md): **Free Play / offline: everything. LAN / private / friends-only: full (gameplay + sync). Public / quick match / server browser: local-only (read, local_ui, cosmetic).** `gate_matrix()` returns the exact table.")
w("")

nfun = 0
toc = []
body = []
def funcs_of(rows): return rows
since11 = set()
core_src = open(os.path.join(ROOT, "src/core/mod_api_core.cpp")).read()
for m in re.finditer(r'BM\((\w+)', core_src): since11.add(m.group(1))
since11.add("goofy_set_override")
since12 = {"goofy_ball_schema", "goofy_player_schema", "goofy_balls", "goofy_ball_state", "goofy_ball_set_velocity", "goofy_ball_set_curve",
           "goofy_ball_impulse", "goofy_players", "goofy_player_state", "goofy_player_cosmetics", "goofy_player_set_override",
           "goofy_player_clear_override", "goofy_sync_status", "content_status", "content_fetch"}
since13 = set()
for m in re.finditer(r'(?:BM\(|D_METHOD\(")(\w+)', open(os.path.join(ROOT, "src/core/mod_api_dev.cpp")).read()): since13.add(m.group(1))
_gs_src = open(os.path.join(ROOT, "src/core/gameplay_settings_api.cpp")).read()
since13 |= set(re.findall(r'D_METHOD\("(\w+)"', _gs_src)) | set(re.findall(r'BM\((\w+)', _gs_src))
since14 = {"audio_list", "audio_buses", "audio_info", "audio_status", "audio_play", "audio_play_loop", "audio_stop", "audio_stop_all", "audio_set_volume", "audio_set_bus", "audio_apply_pack"}
for title, intro, rows in F:
    anchor = re.sub(r'[^a-z0-9 -]', '', title.lower()).replace(' ', '-')
    toc.append(f"- [{title}](#{anchor}) ({len(rows)})")
    body.append(f"## {title}")
    body.append("")
    if intro: body.append(intro); body.append("")
    for (name, params, ret, cap, note, does, ex) in rows:
        nfun += 1
        body.append(f"### {sig(name, params)} → {ret}")
        body.append("")
        ver = "1.4" if name in since14 else ("1.3" if name in since13 else ("1.2" if name in since12 else ("1.1" if name in since11 else "1.0")))
        capids = [c.strip() for c in cap.split("/")] if cap != "-" else []
        if capids:
            c0 = caps.get(capids[0])
            acc = c0["access"] if c0 else "?"
            scope = SCOPE.get(c0["scope"], "") if c0 else ""
            capt = " / ".join(f"`{c}`" for c in capids)
            body.append(f"- **Since** {ver} · **capability** {capt} · **access** `{acc}` · **scope** {scope}" + (f" · _{note}_" if note else ""))
            body.append(f"- **Does / side effects:** {does}")
            body.append(f"- **Multiplayer:** {MP.get(acc, '')}" + (" Undo variant works in every mode." if note.startswith("undo") else ""))
        else:
            body.append(f"- **Since** {ver} · no capability needed (API metadata)" + (f" · _{note}_" if note else ""))
            body.append(f"- **Does / side effects:** {does}")
            body.append("- **Multiplayer:** Safe everywhere (no game state touched).")
        body.append(f"- **Example:** `{ex}`")
        body.append("")

w(f"## Contents ({nfun} functions, 3 signals, {len(caps)} capabilities)")
w("")
out.extend(toc)
w("- [Signals](#signals) · [Events](#events) · [Properties](#properties) · [Config keys and files](#config-keys-and-files) · [Capabilities](#capabilities) · [Script mods and the example](#script-mods-and-the-example) · [Writing a C++ mod](#writing-a-c-mod)")
w("")
out.extend(body)

w("## Signals")
w("")
w("### `access_changed(access: String, open: bool, reason: String)`")
w("- **Since** 1.0. Fires the same frame an access class opens/closes (also on the event bus as `access_changed`). Undo your changes when `open` is false. Fires with everything closed when the core is removed.")
w("- **Example:** `api.access_changed.connect(func(a, o, r): if a == \"offline\" and not o: api.goofy_clear_override(\"my_mod\"))`")
w("")
w("### `event(name: String, data: Dictionary)`")
w("- **Since** 1.1. Every event of the bus, for anyone (no registration). Use `subscribe` to get only some.")
w("- **Example:** `api.event.connect(func(n, d): print(n, d))`")
w("")
w("### `gameplay_setting_changed(id: String, value: Variant, source: String)`")
w("- **Since** 1.3. A registered gameplay setting's effective value changed (also on the event bus as `gameplay_setting_changed`). `source`: default, local, host or stock.")
w("- **Example:** `api.gameplay_setting_changed.connect(func(id, v, src): if id == \"my_mod.gravity\": apply(v))`")
w("")
w("## Events")
w("")
w("| event | capability | data | when |")
w("|---|---|---|---|")
api_core = open(os.path.join(ROOT, "src/api/api_core.cpp")).read()
for m in re.finditer(r'\{ "([a-z_]+)", "([a-z_.]+)", "([^"]*)", "((?:[^"\\]|\\.)*)" \}', api_core):
    w(f"| `{m.group(1)}` | `{m.group(2)}` | `{m.group(3)}` | {m.group(4).replace(chr(92)+chr(34), chr(34))} |")
w("")
w("## Properties")
w("")
w("JVModAPI exposes no settable properties: every state change goes through a checked function so it can be gated, counted and undone. Read-only facts are functions (`get_api_version`, `get_mode`, `is_strict`, ...). The node path `/root/JVModGate/JVModAPI` and the singleton name `JVModAPI` are stable for major 1. `status.json` (user://jv_mods/status.json) carries an `api` summary: `version`, `strict`, `mods`, `coverage`.")
w("")
w("## Config keys and files")
w("")
w("| file / key | type | default | meaning |")
w("|---|---|---|---|")
w("| `mods/api.json` `strict` | bool | false | Strict API mode: only libraries that reach the game through CORE load. Every shipped library is migrated (goofy, shots, loader = textures + models; core UI is part of core), so strict mode refuses only third-party or legacy GDExtension libraries. |")
w("| `mods/api/state/<mod>.json` | JSON object | absent | Per-mod state written by `state_set` (atomic). |")
w("| `mods/api_mods/<name>/mod.gd` | GDScript (extends Node) | absent | A script mod; loaded after the version pin matched, parented under `/root/JVScriptMods/<name>`. |")
w("| `mods/api_mods/<name>/mod.json` `enabled` | bool | true | false = skip this script mod. |")
w("| `mods/core/00-default.json` `devtools` | object | see docs/DEVTOOLS.md | `debugger` (false), `hotkeys` (\"auto\" = build default, or true / false; `profiler_key` F6, `console_key` F11), `overlay` (true), `profiler_seconds` (2), `consts_audit` (true), `log_levels` {tag: level}. |")
w("| `user://jv_profiler/` | folder | - | Profiler captures (`capture-<time>-<n>[-tag].json` + `.csv`) and `debug-dump-<time>.json`. |")
w("| `mods/core/00-default.json` `menu_perf` | bool | true | Menu perf default (cheaper menu motion, same look). `menu_perf_set` changes it for the session. |")
w("| `mods/<category>/*.json` | JSON | per library | Library configs (shots, goofy, textures ...), read by `load_config` / `library_reload`, validated by `config_validate`. |")
w("| stage pack `skin.json` `audio{}` | JSON | - | Optional native ambience for a court (`ambience` = native id/path/alias such as `beach`, plus optional `bus` / `volume`). Parsed only; the stream is loaded when the pack is applied. See docs/PICKERS.md. |")
w("| stage pack `skin.json` `environment{}` / `sky{}` / `court{}` / `goofy{}` | JSON | - | Court environment GLB, distant sky, court look and the map's goofy block (see docs/PICKERS.md, docs/UNIFIED-LOADER.md). Read via `court_env_info`, `loader_map_goofy_block`. |")
w("")
w("## Capabilities")
w("")
w("| id | since | access | scope | status | feature |")
w("|---|---|---|---|---|---|")
for cid, c in caps.items():
    w(f"| `{cid}` | {c['since']} | {c['access']} | {c['scope']} | {c['status']} | {c['feature']} |")
w("")
impl = sum(1 for c in caps.values() if c['status'] == 'impl'); mapped = sum(1 for c in caps.values() if c['status'] == 'mapped'); planned = sum(1 for c in caps.values() if c['status'] == 'planned')
w(f"Coverage: {len(caps)} capabilities, {impl} implemented, {mapped} mapped (reachable through `game.generic`, documented with file:line), {planned} planned. Mapped ones are listed in docs/API.md's migration checklist.")
w("")
w("## Script mods and the example")
w("")
w("`examples/api_mods/hello_api/` is a complete script mod (not installed by the release zip). Copy it to `<game>/mods/api_mods/hello_api/`. It registers, subscribes to events, counts ball contacts through `connect_ball_event`, keeps the count in its state file, and in Free Play puts a gentle low-gravity Goofy layer on that it clears the moment the offline class closes. The pattern:")
w("")
w("```gdscript")
w("var api = Engine.get_singleton(\"JVModAPI\")")
w("api.register_mod(\"my_mod\", \"1.1\", PackedStringArray([\"gate.mode\", \"goofy.query\", \"goofy.ball\"]))")
w("api.access_changed.connect(func(a, open, _r):")
w("    if a == \"offline\" and not open: api.goofy_clear_override(\"my_mod\"))")
w("if api.is_open(\"offline\"): api.goofy_set_override(\"my_mod\", {\"ball.gravity\": {\"mul\": 0.85}})")
w("```")
w("")
w("## Writing a C++ mod")
w("")
w("```cpp")
w("#include \"../common/api_client.hpp\"   // jvm::find_api(), ApiFollower<T>")
w("Object *api = jvm::find_api(this);")
w("api->call(\"register_mod\", \"my_lib\", \"1.1\", PackedStringArray({\"packs.query\", \"stage.pick\"}));")
w("String picked = api->call(\"stage_get_picked\", \"my_lib\");")
w("```")
w("Every shipped library is migrated: `ApiFollower` decides apply / revert from the library's access class and `jvm::GameIO` is its only path to the game:")
w("")
w("```cpp")
w("static jvm::GameIO &IO() { static jvm::GameIO io = [] { jvm::GameIO g; g.mod = \"my_lib\"; g.caps.push_back(\"ball.flight\"); return g; }(); return io; }")
w("Variant curve = IO().get(\"ball.flight\", ball, \"curve\");          // was ball->get(\"curve\")")
w("IO().set(\"ball.flight\", ball, \"curve\", 0.0);                     // needs the class open")
w("IO().call_undo(\"ball.landing_marker\", ball, \"_shard_cluster_mesh\", 0.0, 1.0); // revert path")
w("Node *court = IO().node(\"stage.court_look\", scene, \"Court\");    // stock node path")
w("String shown = IO().core(\"model_get_shown\");                     // another library, through its CORE facade")
w("```")
w("")
w("| library | access class (on/off) | game IO writes | declared capabilities |")
w("|---|---|---|---|")
w("| goofy | gameplay | API facades | goofy.* (see GOOFY-API.md) |")
w("| shots | gameplay | need the class open (revert paths use `undo`) | gate.mode, config.files, scene.*, ball.flight, ball.physics, ball.float, ball.solver, ball.landing_marker, ball.struck, player.input_state, player.input, player.feel |")
w("| textures (court env / sky, custom ball, locker) | cosmetic | declaration-checked | scene.*, net.state, net.member_data, net.packets, ball.model, ball.physics, ball.skin, player.model, player.look, player.outfit, stage.court_look, stage.net_look, court.env, models.query, ui.locker |")
w("| models (model loader, locker tab) | cosmetic | declaration-checked | scene.*, player.model, player.look, ui.locker, ui.style |")
w("| loader (unified loader bridge) | follows its subsystems | declaration-checked | scene.stage, packs.query, goofy.packs |")
w("| core_ui (menu card, picker, mods panel, menu perf) | local UI (core) | declaration-checked | ui.menu, ui.style, ui.notify, ui.mods_panel, menu.perf, stage.picker, stage.pick, ball.pick, ball.skin, packs.*, net.party_ball, lib.* |")
w("")
w("Refusals are logged once per mod and capability (`refused <mod> -> <cap>: <why>`) and counted in `get_mods()`.")
w("")
w("## Decisions")
w("")
w("* `engine.time_scale` (time_scale_set / time_scale_restore) stays a gameplay-class, local Engine.time_scale write for mods. In LAN / private / friends **matches** the time scale follows the **host** (owner decision): Goofy host sync sends the host's time scale with all gameplay params at match start, each client applies it through `time_scale_set`, re-asserts it after the game's own hit-stop slow-mo, and restores its own value at match end or when the host leaves. Public lobbies: local only, nothing sent.")
w("* Everything that alters the ball or a player follows the host in a synced match unless it is visual-only; the per-param classification (`sync`: host / local) is in `goofy_ball_schema` / `goofy_player_schema` and docs/GOOFY-BALL.md, GOOFY-PLAYER.md. Visual-only params and cosmetics stay each player's own.")
w("* Cosmetics are per player and never host-synced: each modded peer advertises what it wears (id + SHA-256 + size, JVMS packets) on join and on every change, everyone renders everyone's outfit, accessories and model; the court and ball everyone plays on are the host's pick. Missing items come only from the embedded assets repo (docs/CONTENT-SHARING.md). Sharing is LAN / private / friends only; public lobbies stay local (no packets, no downloads).")
w("* There is no separate space-platform gravity or underwater drag in the game or Goofy; ball / player gravity and drag are ordinary gameplay params and follow the host.")
w("")

path = os.path.join(ROOT, "docs/CORE-API.md")
open(path, "w").write("\n".join(out) + "\n")
print(f"wrote {path}: {nfun} functions, {len(caps)} capabilities")
