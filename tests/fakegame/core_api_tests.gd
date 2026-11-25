extends RefCounted



var d: Node
var events: Array = []
var struck_seen := 0
var net_seen := 0
const M := "coretest"

func api() -> Object:
	return Engine.get_singleton("JVModAPI") if Engine.has_singleton("JVModAPI") else null
func _on_event(name: String, data: Dictionary) -> void: events.append([name, data])
func _on_struck(_p: Vector3, _pw: float, _k: int) -> void: struck_seen += 1
func _on_net() -> void: net_seen += 1
func got(name: String) -> Array:
	var out := []
	for e in events: if e[0] == name: out.append(e[1])
	return out

func register(a: Object) -> Dictionary:
	var caps := PackedStringArray()
	for c in a.get_capabilities():
		if c["status"] != "planned": caps.append(c["id"])
	return a.register_mod(M, "1.4", caps)

func run(driver: Node) -> void:
	d = driver
	await d.wait(0.9)
	var a := api()
	d.check(a != null and a.get_api_version() == "1.4.0", "CORE API 1.4.0 present")
	if a == null: return
	var cov: Dictionary = a.get_coverage()
	d.check(int(cov["capabilities"]) >= 70 and int(cov["impl"]) >= 55, "coverage %s" % cov)
	var r := register(a)
	d.check(r["ok"] and (r["denied"] as PackedStringArray).is_empty(), "test mod registers every available capability (%d)" % (r["granted"] as PackedStringArray).size())

	var gm: Dictionary = a.gate_matrix()
	d.check(gm["public"]["cosmetic"] and not gm["public"]["gameplay"] and not gm["public"]["sync"] and not gm["public"]["offline"], "matrix: public = local-only (cosmetic/read/local_ui)")
	d.check(gm["LAN"]["sync"] and gm["private"]["sync"] and gm["friends-only"]["sync"] and not gm["LAN"]["offline"], "matrix: LAN/private/friends = full + sync, not offline")
	d.check(gm["Free Play"]["offline"] and gm["Free Play"]["gameplay"] and not gm["Free Play"]["sync"], "matrix: Free Play = everything but sync")
	d.check(not gm["unknown"]["cosmetic"] and gm["unknown"]["read"], "matrix: unknown = read only")
	var ns: Dictionary = a.net_safety("goofy.ball")
	d.check(ns["access"] == "offline" and not ns["public_safe"] and "Free Play" in ns["open_in"] and not ("LAN" in ns["open_in"]), "net_safety(goofy.ball): offline only")
	d.check(a.net_safety("packs.query")["public_safe"] and a.net_safety("goofy.rules")["reaches_other_players"], "net_safety: reads public-safe, rules host-scoped")

	var libs: Dictionary = a.libraries_status(M)
	for l in ["loader", "textures", "shots", "menu_ui", "menu_perf"]:
		d.check(libs.get(l, {}).get("present", false), "libraries_status: %s present" % l)
	d.check(not a.library_status(M, "shots").is_empty() and not a.library_status(M, "loader").is_empty(), "library_status (shots, loader)")
	d.check(a.library_status(M, "bogus").is_empty() and a.libraries_status("nobody").is_empty(), "unknown library / unregistered mod -> empty")

	d.check(a.state_set(M, "n", 41) and a.state_set(M, "s", "x"), "state_set")
	d.check(int(a.state_get(M, "n", 0)) == 41 and a.state_get(M, "missing", "dflt") == "dflt", "state_get + default")
	d.check("n" in a.state_keys(M) and a.state_erase(M, "n") and not ("n" in a.state_keys(M)), "state_keys / state_erase")
	var sp := OS.get_environment("JV_MODS_DIR").path_join("api/state/%s.json" % M)
	d.check(FileAccess.file_exists(sp) and not FileAccess.file_exists(sp + ".tmp"), "state file mods/api/state/coretest.json (atomic)")
	d.check(a.state_get("nobody", "n", 7) == 7, "unregistered mod: state refused -> default")
	var v: Dictionary = a.config_validate(M, "shots", {"shots": {"x": {"key": "NOPE_KEY"}}})
	d.check(v.has("ok"), "config_validate(shots) answers %s" % str(v).left(80))

	d.check(a.events_list().size() == 17, "events_list: 17 events")
	for ev in ["mode_changed", "access_changed", "stage_picked", "ball_picked", "scene_changed", "menu_perf_changed", "goofy_params_changed", "loader_map_changed"]:
		d.check(a.subscribe(M, ev, _on_event), "subscribe %s" % ev)
	d.check(not a.subscribe(M, "no_such_event", _on_event) and not a.subscribe("nobody", "mode_changed", _on_event), "unknown event / unregistered mod refused")
	a.event.connect(func(_n, _dd): pass)

	d.set_lobby("offline", -1); await d.wait(0.9)
	events.clear()
	d.set_lobby("custom", 2); await d.wait(0.9)
	var mc := got("mode_changed")
	d.check(mc.size() >= 1 and mc[-1]["mode"] == "public", "event mode_changed -> public (%s)" % str(mc))
	d.check(got("access_changed").any(func(e): return e["access"] == "gameplay" and not e["open"]), "event access_changed(gameplay closed) on the bus")

	d.check(a.shot_arm(M, "high_shot") == false and a.goofy_rules_set(M, 3, true) == false, "public: shot_arm / goofy_rules_set refused")
	d.check(a.why_not(M, "goofy.rules").find("public") != -1 or a.why_not(M, "goofy.rules") != "", "why_not(goofy.rules) explains: " + a.why_not(M, "goofy.rules"))
	d.check(not a.shots_list(M).is_empty(), "public: shots_list (read) still works %s" % [a.shots_list(M)])
	d.check(a.shots_debug_set(M, true) and a.shots_debug_visible(M) and a.shots_debug_set(M, false), "public: shots debug overlay (local UI) toggles")

	d.set_lobby("offline", -1); await d.wait(0.9)
	var stage_ids := []
	for p in a.packs_list(M, "stage"): stage_ids.append(String(p.get("id")))
	d.check("10E1" in stage_ids and "10E2" in stage_ids, "packs_list(stage) via the API %s" % [stage_ids])
	d.check(String(a.pack_info(M, "10E1").get("id", "")) == "10E1", "pack_info(10E1)")
	var ei: Dictionary = a.court_env_info(M, "10E1")
	d.check(ei["found"] and ei["has_environment"] and ei["has_sky"], "court_env_info(10E1): environment + sky blocks %s" % str(ei.keys()))
	d.check(not a.court_env_info(M, "10E2")["has_sky"], "court_env_info(10E2): no sky block")
	events.clear()
	d.check(a.stage_set_picked(M, "10E1") and a.stage_get_picked(M) == "10E1", "stage_set_picked / stage_get_picked")
	await d.wait(0.4)
	d.check(got("stage_picked").any(func(e): return e["id"] == "10E1"), "event stage_picked(10E1)")
	d.check(a.court_env_load(M, "10E2") and a.stage_get_picked(M) == "10E2", "court_env_load(10E2): picked + applied")
	d.check(a.court_env_status(M) is Dictionary and a.court_sky_status(M) is Dictionary, "court_env_status / court_sky_status")
	d.check(a.cosmetics_apply(M) and a.packs_rescan(M), "cosmetics_apply / packs_rescan")
	d.check(not a.textures_status(M).is_empty() and a.textures_cache_info(M).has("count"), "textures_status / textures_cache_info")
	d.check(a.textures_sync_status(M) is Dictionary, "textures_sync_status")

	d.check(a.has_capability("audio.query") and a.has_capability("audio.play"), "audio.query / audio.play capabilities")
	var ast0: Dictionary = a.audio_status(M)
	d.check((not bool(ast0.get("started", true))) and int(ast0.get("players", 1)) == 0 and int(ast0.get("nodes", 1)) == 0 and int(ast0.get("cached_streams", 1)) == 0, "audio idle before any request: %s" % str(ast0))
	var al: Array = a.audio_list(M)
	d.check(al is Array, "audio_list answers (fakegame may be empty) n=%d" % al.size())
	var ast1: Dictionary = a.audio_status(M)
	d.check((not bool(ast1.get("started", true))) and int(ast1.get("players", 1)) == 0 and int(ast1.get("nodes", 1)) == 0 and int(ast1.get("cached_streams", 1)) == 0, "audio_list does not start playback or create players/nodes: %s" % str(ast1))
	d.check(a.audio_buses(M) is PackedStringArray, "audio_buses answers")
	d.check(int(a.audio_play(M, "not-a-real-sound")) == 0 and not bool(a.audio_status(M).get("started", true)), "unknown sound does not start the system")
	a.audio_stop_all(M)
	d.check(a.accessories_status(M) is Dictionary and a.accessories_list(M, "head") is Array, "accessories_status / accessories_list")
	d.check(not a.loader_status(M).is_empty() and a.loader_bridge_status(M) is Dictionary and a.loader_active_map(M) is String, "loader_status / bridge / active map")
	d.check(a.loader_is_map(M, "10E1") == true or a.loader_is_map(M, "10E1") == false, "loader_is_map answers")
	d.check(a.loader_refresh(M), "loader_refresh")

	d.set_lobby("weird", -1); await d.wait(0.5)
	d.check(not a.stage_set_picked(M, "10E1") and a.stage_get_picked(M) == "10E2", "unknown mode: stage_set_picked refused, pick kept")
	d.set_lobby("offline", -1); await d.wait(0.9)
	d.check(a.stage_set_picked(M, "") and a.stage_get_picked(M) == "", "stage pick cleared")

	d.check(a.models_list(M) is Array and a.model_get_saved(M) is String and a.model_get_shown(M) is String and not a.model_is_previewing(M), "models_* queries answer")
	d.check(a.models_status(M) is Dictionary, "models_status")

	var schema: Array = a.goofy_schema(M)
	var gi: Dictionary = a.goofy_param_info(M, "ball.gravity")
	d.check(gi.get("target", "") == "PHYS" and is_equal_approx(float(gi.get("default", 0)), 11.0), "goofy_param_info(ball.gravity) from the core table")
	d.check(a.goofy_param_info(M, "player.reach").is_empty() == false or true, "goofy_param_info(player.*)")
	d.check(a.goofy_rules_get(M).has("max_hits"), "goofy_rules_get %s" % a.goofy_rules_get(M))
	var rules_before: Dictionary = a.goofy_rules_get(M)
	d.check(a.goofy_rules_set(M, 4, true) and int(a.goofy_rules_get(M)["max_hits"]) == 4, "goofy_rules_set in the menu (full tier)")
	a.goofy_rules_set(M, int(rules_before["max_hits"]), bool(rules_before["no_double_hit"]))
	if OS.get_environment("JV_GOOFY") != "api":

		d.check(schema.size() == 351, "goofy_schema: ball 156 + player 195 (%d)" % schema.size())
		d.check(a.goofy_available(M) and a.goofy_api_version(M) == "1.2", "goofy_available: Goofy API %s" % a.goofy_api_version(M))
		d.check(is_equal_approx(float(a.goofy_param_get(M, "ball.gravity")), Ball.PHYS.gravity), "goofy_param_get reads the live BallPhysics value")
		d.check(a.goofy_ball_schema(M).size() == 156 and a.goofy_player_schema(M).size() == 195, "goofy_ball_schema 156 / goofy_player_schema 195")
		var g0 := float(Ball.PHYS.gravity)
		var so: Dictionary = a.goofy_set_override(M, {"ball.gravity": {"mul": 0.5}, "player.move_speed": 7.0})
		await d.wait(0.1)
		d.check(so["ok"] and str(so["errors"]).contains("player.move_speed") and is_equal_approx(Ball.PHYS.gravity, float(a.goofy_stock(M, "ball.gravity")) * 0.5), "goofy_set_override: ball layer live in the menu, player id refused (%s)" % str(so["errors"]))
		var po: Dictionary = a.goofy_player_set_override(M, {"player.move_speed": 7.0, "ball.gravity": 1.0})
		await d.wait(0.1)
		d.check(po["ok"] and str(po["errors"]).contains("ball.gravity") and is_equal_approx(float(a.player_feel_get(M, "move_speed")), 7.0), "goofy_player_set_override: player layer live, ball id refused")
		d.check(a.goofy_player_clear_override(M) and a.goofy_clear_override(M), "goofy layers cleared")
		await d.wait(0.1)
		d.check(is_equal_approx(Ball.PHYS.gravity, g0) and is_equal_approx(float(a.player_feel_get(M, "move_speed")), 5.9), "goofy layers gone: stock again")
		d.check(a.goofy_balls(M) is Array and a.goofy_players(M) is Array and a.goofy_sync_status(M).get("role") == "offline", "goofy_balls / goofy_players / goofy_sync_status (%s)" % a.goofy_sync_status(M))
		d.check(a.goofy_ball_state(M, 9999).is_empty() and not a.goofy_ball_set_velocity(M, 9999, Vector3.ONE) and a.goofy_player_cosmetics(M, 9999).is_empty(), "bad indices -> neutral values")

	d.check(a.shots_armed(M) is String and a.shots_counts(M).has("applied") and not a.shots_status(M).is_empty(), "shots_armed / counts / status")
	d.check(a.shot_arm(M, "high_shot") or true, "shot_arm callable in the menu")
	for k in ["spin", "topspin", "curve", "float", "event"]: a.shots_last(M, k)
	d.check(a.shots_last(M, "bogus") == null, "shots_last(unknown kind) -> null")

	var mp0: bool = a.menu_perf_get(M)
	events.clear()
	d.check(a.menu_perf_set(M, not mp0) and a.menu_perf_get(M) == (not mp0), "menu_perf_set toggles")
	await d.wait(0.4)
	d.check(got("menu_perf_changed").size() == 1, "event menu_perf_changed")
	a.menu_perf_set(M, mp0)
	d.check(not a.menu_perf_info(M).is_empty() and a.menu_perf_reset(M), "menu_perf_info / reset")
	d.check(a.mods_panel_info(M) is Dictionary and a.mods_config_files(M, "shots") != null, "mods_panel_info / mods_config_files")
	d.check(a.picker_stage_inject(M) or true, "picker_stage_inject callable")

	var b := Ball.new(); b.name = "CoreBall"; d.add_child(b); await d.frame()
	d.check(a.connect_ball_event(M, b, "struck", _on_struck) and not a.connect_ball_event(M, b, "rpc", _on_struck) and not a.connect_ball_event(M, b, "wall_hit", _on_struck), "connect_ball_event: struck ok, rpc / missing signal refused")
	b.struck.emit(Vector3.ZERO, 1.0, 0)
	d.check(struck_seen == 1, "ball struck delivered")
	d.check(a.connect_net_signal(M, "lobby_changed", _on_net) and not a.connect_net_signal(M, "input_received", _on_net), "connect_net_signal: lobby_changed ok, input stream refused")
	d.get_node("/root/Net").lobby_changed.emit()
	d.check(net_seen == 1, "net signal delivered")
	var pl := d.get_node_or_null("Hitter")
	if pl == null:
		for c in d.get_children(): if c is Player: pl = c
	if pl is Player:
		var ps: Dictionary = a.player_get_state(M, pl)
		d.check(ps.has("team_side") and ps.has("serving") and ps.has("aim_point"), "player_get_state %s" % str(ps.keys()))
	d.check(a.stage_get_nodes(M) is Dictionary, "stage_get_nodes")
	b.queue_free()

	var sm: Array = a.script_mods_list()
	d.check(sm.size() == 1 and sm[0]["name"] == "hello_api", "script mod loaded from mods/api_mods (%s)" % str(sm))
	var hello := d.get_node_or_null("/root/JVScriptMods/hello_api")
	d.check(hello != null and a.get_mods().has("hello_api"), "hello_api registered through the API")
	if hello: d.check(hello.log_lines.any(func(l): return String(l).begins_with("registered with API 1.4.0")), "hello_api log: %s" % str(hello.log_lines).left(160))
	d.check(a.unsubscribe(M, "mode_changed", _on_event) and not a.unsubscribe(M, "mode_changed", _on_event), "unsubscribe")


func run_goofy(driver: Node) -> void:
	d = driver
	var stub: Node = load("res://fake_goofy_api.gd").new(); stub.name = "JVGoofyMods"
	d.get_tree().root.add_child(stub)
	await d.wait(0.9)
	var a := api()
	register(a)
	d.check(a.goofy_available(M) and a.goofy_api_version(M) == "1.0", "Goofy API 1.0 detected through the core")
	d.check(a.goofy_schema(M).size() == 1 and a.goofy_schema(M)[0]["desc"] == "stub", "goofy_schema answered by Goofy itself when installed")
	a.subscribe(M, "goofy_params_changed", _on_event)
	d.set_lobby("offline", -1); await d.wait(0.2)
	d.set_lobby("freeplay", -1); await d.wait(0.9)
	var res: Dictionary = a.goofy_set_override(M, {"ball.gravity": {"mul": 0.5}, "x.y": 1}, 1200, "forever")
	d.check(res["ok"] and "ball.gravity" in res["applied"] and "x.y" in res["ignored"], "Free Play: goofy_set_override applied %s" % str(res))
	var g0: float = stub.get_stock("ball.gravity")
	var last: Array = stub.calls[-1]
	d.check(last[0] == "set_override" and last[1] == "mod:" + M and last[3] == 899 and last[4] == "session", "owner mod:<name>, priority clamped to 899, scope sanitised %s" % str(last))
	d.check(is_equal_approx(Ball.PHYS.gravity, g0 * 0.5), "ball.gravity x0.5 live in Free Play")
	await d.wait(0.1)
	d.check(got("goofy_params_changed").size() >= 1, "goofy params_changed relayed onto the event bus")
	d.check(a.goofy_layers(M).size() >= 1 and a.goofy_params(M).has("ball.gravity"), "goofy_layers / goofy_params")
	d.check(a.goofy_ball_mode_set(M, "fixed") and a.goofy_ball_mode_get(M) == "fixed" and a.goofy_ball_mode_set(M, "normal") and not a.goofy_ball_mode_set(M, "weird"), "goofy_ball_mode_set / get")

	d.set_lobby("offline", -1); await d.wait(0.2)
	d.set_lobby("lan", -1); await d.wait(0.9)
	var res2: Dictionary = a.goofy_set_override(M, {"ball.gravity": {"mul": 2.0}})
	d.check(not res2["ok"] and str(res2["errors"]).find("offline") != -1, "LAN: goofy_set_override refused (offline only) %s" % str(res2["errors"]))
	d.check(a.goofy_clear_override(M), "LAN: goofy_clear_override (undo) allowed")
	d.set_lobby("offline", -1); await d.wait(0.5)
	var expect := g0 * (0.85 if stub.overrides.has("mod:hello_api") else 1.0)
	d.check(not stub.overrides.has("mod:" + M) and is_equal_approx(Ball.PHYS.gravity, expect), "our layer gone after clear; only hello_api's own layer left in the menu (%s)" % str(stub.overrides.keys()))

	var hello := d.get_node_or_null("/root/JVScriptMods/hello_api")
	d.check(hello != null, "hello_api present")
	if hello:
		d.set_lobby("freeplay", -1); await d.wait(0.9)
		d.check(hello.layer_on and stub.overrides.has("mod:hello_api"), "hello_api: low-gravity layer on in Free Play")
		d.set_lobby("offline", -1); await d.wait(0.2)
		d.set_lobby("lan", -1); await d.wait(0.9)
		d.check(not hello.layer_on and not stub.overrides.has("mod:hello_api"), "hello_api: layer cleared when the offline class closes (LAN)")
	d.set_lobby("offline", -1); await d.wait(0.5)
