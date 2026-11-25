extends Node3D



























var _practice_solo := false
var _practice_on := false
var _freeplay := false
var _freeplay_stage := ""
var last_swap_stage := ""
var _drill_active := ""
var _member_queuing := false
var _qp = null
var _peer_players := {}
var player: Node = null

var ui: CanvasLayer = null
var _home: Control = null
var _discord_btn: Button = null
var _tilt: Array = []
var _bottom_bar: HBoxContainer = null
var credits_opened := 0
var _rally = null

var fails := 0
var passes := 0
var mode := OS.get_environment("JV_TEST_MODE")

func _alb(m: Material) -> Color:
	if m is ShaderMaterial: return (m as ShaderMaterial).get_shader_parameter("albedo")
	if m is BaseMaterial3D: return (m as BaseMaterial3D).albedo_color
	return Color(-1, -1, -1)

func check(c: bool, what: String) -> void:
	if c: passes += 1; print("PASS ", what)
	else: fails += 1; print("FAIL ", what)
func wait(s: float) -> void:
	await get_tree().create_timer(s).timeout
func frame() -> void:
	await get_tree().process_frame
func gate() -> Node: return get_node_or_null("/root/JVModGate")
func tl() -> Node: return get_node_or_null("/root/JVTextureLoader")
func gravity() -> float:
	return PhysicsServer3D.area_get_param(get_world_3d().space, PhysicsServer3D.AREA_PARAM_GRAVITY)
func status() -> Dictionary:
	var t := FileAccess.get_file_as_string("user://jv_mods/status.json")
	var v = JSON.parse_string(t)
	return v if v is Dictionary else {}
var base_g := 0.0
func full_on() -> bool:
	return InputMap.has_action("jvm_shot_jump_serve") and is_equal_approx(Engine.time_scale, full_time_scale()) and $Hitter.scale.x > 2.9 and is_equal_approx(gravity(), base_g)



func full_time_scale() -> float:
	return 0.5
func full_off() -> bool:
	return not InputMap.has_action("jvm_shot_jump_serve") and is_equal_approx(Engine.time_scale, 1.0) and is_equal_approx($Hitter.scale.x, 1.0) and is_equal_approx(gravity(), base_g)
func tex_on() -> bool: return tl() != null and tl().get_swapped_count() == 1
func finish() -> void:
	print("RESULT %d passed, %d failed" % [passes, fails])
	get_tree().quit(1 if fails else 0)

func _swap_stage(id: String, broadcast := true, persist := true, force := false, defer_arena := false, tod := -1) -> void:
	last_swap_stage = id
	_freeplay_stage = id
	if Engine.has_singleton("GameConfig") or get_node_or_null("/root/GameConfig"):
		var gc = get_node_or_null("/root/GameConfig")
		if gc: gc.set("freeplay_stage", id)

func reset_menu() -> void:
	_practice_solo = false; _practice_on = false; _freeplay = false; _drill_active = ""; _member_queuing = false
	Net.active = false; Net.custom_lobby = false; Net.party_lan = false; Net._lobby_kind = "custom"
	Net.privacy = 0; Net.password = ""; Net.steam_lobby_id = 0
	GameConfig.quick_match = false; GameConfig.ranked_match = false; GameConfig.net_roster = []
	Net.in_match = false; Net.practice_live = false

func _ready() -> void:
	player = $Hitter
	if mode == "plainhost" or mode == "plainclient":
		await _plain(mode == "plainhost"); return
	await wait(0.3)
	base_g = gravity()
	if mode == "pinfail":
		await _pinfail(); return
	if mode == "nethost" or mode == "netclient":
		await _net(); return
	if mode == "curvehost" or mode == "curveclient":
		await _curve_display_net(); return
	check(gate() != null, "core gate node present")
	if mode == "apistrict":
		await load("res://api_tests.gd").new().run_strict(self); finish(); return
	for n in ["JVTextureLoader", "JVExtraShots", "JVGoofyMods", "JVModelLoader"]:
		if n == "JVGoofyMods" and OS.get_environment("JV_GOOFY") in ["api", "none"]: continue
		check(get_node_or_null("/root/" + n) != null, n + " node present")
	check(gate().pin_ok(), "version pin OK (%s)" % gate().get_pin_result().get("reason"))
	if mode == "jvnet": await _jvnet()
	elif mode == "gameplay": await _gameplay()
	elif mode == "menuui": await _menuui()
	elif mode == "models": await _models()
	elif mode == "pickers": await _pickers()
	elif mode == "loader": await _loader()
	elif mode == "perf": await _perf()
	elif mode == "menuperf": await _menuperf()
	elif mode == "pconsts": await _pconsts()
	elif mode == "api": await load("res://api_tests.gd").new().run(self)
	elif mode == "apicore": await load("res://core_api_tests.gd").new().run(self)
	elif mode == "gameplaysettings": await load("res://gameplay_settings_tests.gd").new().run(self)
	elif mode == "apicore-goofy": await load("res://core_api_tests.gd").new().run_goofy(self)
	elif mode == "devtools": await load("res://devtools_tests.gd").new().run(self)
	else: await _generic()
	finish()


func set_lobby(kind: String, vis: int) -> void:
	Lobby.match_kind = kind; Lobby.lobby_type = vis
func _generic() -> void:
	await wait(0.9)
	check(gate().is_active() and gate().is_cosmetic_active() and full_on() and tex_on(), "menu -> everything ON (local)")
	var cases := [

		["custom", 2, false, true, "public custom lobby"],
		["custom", 3, false, true, "invisible (searchable) lobby"],
		["quick", -1, false, true, "quick match"],
		["browser", -1, false, true, "server browser"],
		["custom", -1, false, false, "custom lobby, unknown visibility"],
		["weird", -1, false, false, "unknown context"],
		["custom", 0, true, true, "private custom lobby"],
		["custom", 1, true, true, "friends-only custom lobby"],
		["lan", -1, true, true, "LAN match"],
		["freeplay", -1, true, true, "free play (generic map)"],
	]
	for c in cases:
		set_lobby("offline", -1); await wait(0.2)
		set_lobby(c[0], c[1]); await wait(0.9)
		check(gate().is_active() == c[2] and gate().is_cosmetic_active() == c[3], "%s -> full %s cosmetic %s (%s)" % [c[4], c[2], c[3], gate().get_reason()])
		check((full_on() if c[2] else full_off()) and tex_on() == c[3], c[4] + ": library states match tiers")
	set_lobby("custom", 0); await wait(0.9)
	await frame()
	Input.action_press("jvm_shot_high_shot")
	await frame(); await get_tree().physics_frame
	Input.action_release("jvm_shot_high_shot")
	check($Ball.linear_velocity.length() > 5.0, "high_shot applied velocity %s" % $Ball.linear_velocity)
	set_lobby("custom", 2)
	var f0 := Engine.get_process_frames()
	while gate().is_active() and Engine.get_process_frames() - f0 < 10: await frame()
	var took := Engine.get_process_frames() - f0
	check(not gate().is_active() and full_off() and took <= 1, "private -> public: full OFF on next frame (took %d)" % took)
	check(tex_on(), "private -> public: textures (cosmetic) stay on")
	set_lobby("offline", -1); await frame(); await frame()
	await wait(0.9)
	check(tex_on() and full_on(), "public -> menu: textures stay on, full back on")
	set_lobby("custom", -1); await frame(); await frame()
	check(not tex_on(), "menu -> unidentified lobby: textures reverted (fail closed)")
	set_lobby("custom", 0); await wait(0.9)
	check(full_on(), "re-enabled in private lobby")
	gate().queue_free(); await frame(); await frame()
	check(full_off() and not tex_on(), "core gate removed -> all libraries revert (fail closed)")


func expect(label: String, full: bool, cos: bool, mode_label: String) -> void:
	await wait(0.9)
	var st := status()
	check(gate().is_active() == full and gate().is_cosmetic_active() == cos and gate().get_mode() == mode_label,
		"%s -> mode '%s' full %s cosmetic %s (got '%s' %s/%s: %s)" % [label, mode_label, full, cos, gate().get_mode(), gate().is_active(), gate().is_cosmetic_active(), gate().get_reason()])
	check((full_on() if full else full_off()) and tex_on() == cos, label + ": libraries follow tiers")
	var libs: Dictionary = st.get("libraries", {})
	check(st.get("mode") == mode_label and libs.get("shots", {}).get("state") == ("active" if full else "blocked")
		and libs.get("textures", {}).get("state") == ("active" if cos else "blocked") and libs.get("core", {}).get("state") == "active",
		label + ": status.json mode/libraries")



func switch_frames(_cond: Callable, key := "full") -> int:
	var f0 := Engine.get_process_frames()
	for i in 4: await frame()
	var st := status()
	var cf: int = int(st.get("gate", {}).get(key, {}).get("changed_frame", -1))
	var mf: int = int(st.get("mode_changed_frame", -1))
	print("  change at frame %d, gate %s switched at %d, status mode changed at %d" % [f0, key, cf, mf])
	return max(cf, mf) - f0 if cf >= f0 else 99

func _jvnet() -> void:
	reset_menu(); await expect("idle main menu", true, true, "menu")
	check(gate().get_reason() == "allowed: main menu (local)", "menu reason: " + gate().get_reason())
	var added := "res://mods/resources/packs/cafe skin pack/add/extra tex.png"
	check(ResourceLoader.has_cached(added), "added texture with spaces in path registered: " + added)
	check(int(tl().get_status().get("packs", 0)) >= 2, "packs loaded from 'resources/packs' folder (%s)" % str(tl().get_status().get("packs", 0)))
	check(tex_on() and tl().get_sync_status().get("network") == "off", "menu: textures on, texture network off")
	check(status().get("libraries", {}).get("textures", {}).get("state") == "active" and status().get("libraries", {}).get("shots", {}).get("state") == "active", "menu: status.json textures + shots active")

	Net.active = true
	var tk := await switch_frames(func(): return true, "cosmetic")
	check(tk <= 1 and not gate().is_cosmetic_active() and not tex_on() and full_off(), "menu -> joining: everything OFF in %d frame(s) (fail closed)" % tk)
	reset_menu(); await expect("menu again", true, true, "menu")

	var cf0: int = int(status().get("gate", {}).get("full", {}).get("changed_frame", -1))
	_practice_solo = true; await expect("menu -> Free Play", true, true, "Free Play")
	reset_menu(); await expect("Free Play -> menu", true, true, "menu")
	var shot_actions := InputMap.get_actions().filter(func(a): return str(a).begins_with("jvm_shot_"))
	check(int(status().get("gate", {}).get("full", {}).get("changed_frame", -1)) == cf0 and shot_actions.size() == 11,
		"menu -> Free Play -> menu: gate never switched, %d shot actions (expect 11 = the 11 default shots incl. curveball + float, no duplicates)" % shot_actions.size())
	reset_menu(); _practice_solo = true; await expect("Free Play (solo practice court)", true, true, "Free Play")
	await frame()
	$Ball.linear_velocity = Vector3.ZERO
	Input.action_press("jvm_shot_high_shot")
	await frame(); await get_tree().physics_frame
	Input.action_release("jvm_shot_high_shot")
	check($Ball.linear_velocity.length() > 5.0, "Free Play: extra shot (high_shot) works, ball velocity %s" % $Ball.linear_velocity)
	reset_menu(); _freeplay = true; await expect("free roam (_freeplay)", true, true, "Free Play")
	reset_menu(); _freeplay = true; _drill_active = "serve_basics"; await expect("practice drill", true, true, "practice")
	reset_menu(); _freeplay = true; _drill_active = "tutorial"; await expect("tutorial", true, true, "practice")

	var cfg_path := OS.get_environment("JV_GATE_CONFIG")
	var cfg = JSON.parse_string(FileAccess.get_file_as_string(cfg_path))
	cfg["probes"][0]["menu_scene"] = "res://not_menu.tscn"; cfg["probes"][0]["main_scene"] = "res://main.tscn"
	var f := FileAccess.open(cfg_path, FileAccess.WRITE); f.store_string(JSON.stringify(cfg)); f.close()
	gate().reload_config(); reset_menu(); await expect("offline solo match (main scene, no network)", true, true, "offline solo")
	GameConfig.net_roster = [1, 2]; await expect("main scene with stale net roster", false, false, "unknown")
	cfg["probes"][0]["menu_scene"] = "res://main.tscn"; cfg["probes"][0]["main_scene"] = "res://game/main/main.tscn"
	f = FileAccess.open(cfg_path, FileAccess.WRITE); f.store_string(JSON.stringify(cfg)); f.close()
	gate().reload_config()

	reset_menu(); Net.active = true; Net.custom_lobby = true; Net.privacy = 0; await expect("public custom lobby", false, true, "public")
	reset_menu(); Net.active = true; Net.custom_lobby = true; Net.privacy = 2; Net.password = "hunter2"; await expect("password private lobby", true, true, "private")
	reset_menu(); Net.active = true; Net.custom_lobby = true; Net.privacy = 2; await expect("invite-only private lobby", true, true, "private")
	reset_menu(); Net.active = true; Net.custom_lobby = true; Net.privacy = 1; await expect("friends-only lobby", true, true, "friends-only")
	reset_menu(); Net.active = true; Net.party_lan = true; await expect("LAN (party_lan)", true, true, "LAN")
	reset_menu(); Net.active = true; GameConfig.quick_match = true; await expect("quick match", false, true, "quick match")
	reset_menu(); Net.active = true; Net._lobby_kind = "locked"; await expect("quick match lobby (_lobby_kind locked)", false, true, "quick match")
	reset_menu(); Net.active = true; GameConfig.quick_match = true; GameConfig.ranked_match = true; await expect("ranked match (v1.0.4)", false, true, "quick match")
	reset_menu(); Net.active = true; Net.custom_lobby = true; Net.privacy = 2; GameConfig.ranked_match = true; await expect("ranked flag wins over a private-looking lobby", false, true, "quick match")
	reset_menu(); Net.active = true; Net.custom_lobby = true; Net.privacy = 7; await expect("custom lobby, unreadable privacy", false, false, "unknown")
	reset_menu(); Net.active = true; await expect("online session, not a custom lobby (party/joining)", false, false, "unknown")
	reset_menu(); _practice_solo = true; _member_queuing = true; await expect("Free Play while queuing for Quick Match", false, true, "quick match")
	reset_menu(); _practice_solo = true; Net.steam_lobby_id = 1234; await expect("Free Play with a Steam lobby opening", false, false, "unknown")

	reset_menu(); _practice_solo = true; await expect("Free Play (before hosting)", true, true, "Free Play")
	Net.active = true; Net.custom_lobby = true; Net.privacy = 0
	var took := await switch_frames(func(): return true)
	check(took <= 1 and full_off(), "Free Play -> host public lobby: full OFF in %d frame(s)" % took)
	check(status().get("mode") == "public", "Free Play -> host public: status.json says public")
	await expect("hosted public lobby", false, true, "public")

	reset_menu(); _practice_solo = true; await expect("Free Play (before joining)", true, true, "Free Play")
	Net.active = true
	took = await switch_frames(func(): return true, "cosmetic")
	check(took <= 1 and full_off(), "Free Play -> joining: everything OFF in %d frame(s) while lobby unknown" % took)
	Net.custom_lobby = true; Net.privacy = 0
	await expect("joined public lobby", false, true, "public")

	reset_menu(); Net.active = true; Net.custom_lobby = true; Net.privacy = 2; await expect("private lobby (before switch)", true, true, "private")
	Net.privacy = 0
	took = await switch_frames(func(): return true)
	var st := status()
	check(took <= 1 and full_off() and st.get("mode") == "public" and st["libraries"]["shots"]["state"] == "blocked",
		"private -> public: gate + status.json updated within 1 frame (took %d, file frame %s)" % [took, st.get("frame")])
	Net.privacy = 2
	await wait(0.9)
	check(status().get("mode") == "private", "public -> private: status.json back to private")

	gate().set_overlay_visible(true); await frame()
	var lbl: Label = gate().get_child(0).get_child(0)
	check(lbl.visible and lbl.text.find("mode: private") != -1, "F8 overlay shows mode")
	var ev := InputEventKey.new(); ev.keycode = KEY_F8; ev.pressed = true
	Input.parse_input_event(ev); await frame(); await frame()
	check(not lbl.visible, "F8 toggles overlay off")
	check(status().get("version_pin", {}).get("ok") == true, "status.json version_pin ok")
	check(str(status().get("timestamp", "")) != "", "status.json timestamp present")

	check(tl().has_method("get_status_brief") and String(tl().get_status_brief().get("state", "")) != "",
		"textures get_status_brief is state/reason only (loader must not call full get_status)")
	check(tl().has_method("get_status_rebuild_count"), "textures exposes get_status_rebuild_count")
	var _st_warm: Dictionary = tl().get_status()
	var rebuilds := int(tl().get_status_rebuild_count())
	var _st_cached: Dictionary = tl().get_status()
	check(int(tl().get_status_rebuild_count()) == rebuilds, "second get_status with no changes is cached")
	check(int(_st_warm.get("packs", -1)) == int(_st_cached.get("packs", -2)), "cached get_status keeps pack count")
	var mip1: Dictionary = tl().get_mip_stats()
	var mip2: Dictionary = tl().get_mip_stats()
	check(int(mip1.get("textures", -1)) == int(mip2.get("textures", -2)) and int(mip1.get("bytes", -1)) == int(mip2.get("bytes", -2)),
		"get_mip_stats is cached / stable (no per-call get_data copy)")
	if gate().has_method("get_last_status_build_ms"):
		print("STATUSBENCH last_status_build_ms=", snapped(gate().get_last_status_build_ms(), 0.001),
			" cached=", gate().status_was_cached_build() if gate().has_method("status_was_cached_build") else "?")
	check(not FileAccess.file_exists("user://jv_mods/status.json.tmp"), "no leftover status tmp file (atomic rename)")
	check(gate().has_method("status_write_through") and gate().status_write_through() == false,
		"status.json heartbeat does not use WRITE_THROUGH")

	check(tl().get_override_count() >= 1, "local player skin applied as material override (%d)" % tl().get_override_count())
	reset_menu(); await expect("back to menu", true, true, "menu")
	check(tex_on() and tl().get_sync_status().get("network") == "off", "back in menu: textures stay on, local only")


func _pinfail() -> void:
	check(gate() != null and not gate().pin_ok(), "core loaded but pin refused (%s)" % (gate().get_pin_result().get("reason") if gate() else "no gate"))
	for n in ["JVTextureLoader", "JVExtraShots", "JVGoofyMods", "JVModelLoader"]:
		check(get_node_or_null("/root/" + n) == null, n + " NOT loaded")
	Net.active = true; Net.custom_lobby = true; Net.privacy = 2
	await wait(1.0)
	check(not gate().is_active() and not gate().is_cosmetic_active(), "gate never opens without a pin")
	var st := status()
	check(st.get("libraries", {}).get("textures", {}).get("state") == "disabled-version-mismatch" and st["libraries"]["core"]["state"] == "disabled-version-mismatch", "status.json shows disabled-version-mismatch")
	check(st.get("version_pin", {}).get("ok") == false, "status.json version_pin ok=false")
	var banner: Label = gate().get_child(0).get_child(1)
	check(banner.visible and banner.text.begins_with("just volleyball mods disabled: game version changed (expected build"), "on-screen banner: " + banner.text)
	check(full_off(), "nothing applied")
	finish()



var PORT: int = int(OS.get_environment("JV_NET_PORT")) if OS.get_environment("JV_NET_PORT").is_valid_int() else 47911
var jv_rx := [0]
var _gate_cfg_orig := ""
func _net_lobby_setup(lobby: String) -> void:

	var cfg := { "max_age_s": 2.0, "probes": [
		{ "type": "node_property", "name": "fake-context", "authoritative": true, "node": "/root/Lobby", "property": "match_kind", "role": "context",
		  "map": { "custom": "custom_lobby", "quick": "quick_match", "browser": "server_browser" } },
		{ "type": "node_property", "name": "fake-visibility", "authoritative": true, "node": "/root/Lobby", "property": "lobby_type", "role": "visibility",
		  "map": { "0": "private", "1": "friends_only", "2": "public" } } ] }
	if _gate_cfg_orig == "": _gate_cfg_orig = FileAccess.get_file_as_string(OS.get_environment("JV_GATE_CONFIG"))
	var f := FileAccess.open(OS.get_environment("JV_GATE_CONFIG"), FileAccess.WRITE); f.store_string(JSON.stringify(cfg)); f.close()
	gate().reload_config()
	match lobby:
		"public": set_lobby("custom", 2)
		"quick": set_lobby("quick", -1)
		"browser": set_lobby("browser", -1)
		_: set_lobby("custom", 0)

func _net() -> void:
	var lobby := OS.get_environment("JV_NET_LOBBY")
	if lobby == "": lobby = "lan"
	if lobby != "lan": await _net_lobby_setup(lobby)
	if OS.get_environment("JV_PLAYER_CONSTS") == "1":
		await _net_pc(lobby); return
	if OS.get_environment("JV_GOOFY_SYNC") == "1":
		await _net_goofy(lobby); return
	if OS.get_environment("JV_CONTENT") == "1":
		await _net_content(); return
	if lobby in ["public", "quick", "browser"]:
		await _net_public(lobby); return
	var peer := ENetMultiplayerPeer.new()
	var host := mode == "nethost"
	var expect_missing := OS.get_environment("JV_EXPECT_MISSING")
	if host:
		peer.create_server(PORT, 4)
		Net.players = {1: {"name": "Hosty"}}
		multiplayer.peer_connected.connect(func(id): Net.players[id] = {"name": "Clienty"}; _peer_players[id] = $Other)
	else:
		peer.create_client("127.0.0.1", PORT)
	multiplayer.multiplayer_peer = peer
	(multiplayer as SceneMultiplayer).peer_packet.connect(func(_id, p: PackedByteArray): if p.size() >= 4 and p.slice(0, 4).get_string_from_ascii() == "JVMS": jv_rx[0] += 1)
	Net.active = true; Net.party_lan = lobby == "lan"; Net.custom_lobby = lobby != "lan"; Net.is_host = host
	_peer_players[multiplayer.get_unique_id()] = $Hitter
	if not host: _peer_players[1] = $Other
	var want_other := OS.get_environment("JV_EXPECT_OTHER_SKIN") == "1"
	var want_channel := OS.get_environment("JV_EXPECT_CHANNEL")
	var t0 := Time.get_ticks_msec()
	var t := 0.0
	var ok := false
	var limit := float(OS.get_environment("JV_NET_WAIT")) if OS.get_environment("JV_NET_WAIT") != "" else 25.0
	while t < limit:
		await wait(0.5); t = (Time.get_ticks_msec() - t0) / 1000.0
		if tl() == null: break
		var lines: PackedStringArray = tl().get_missing_lines()
		var chan_ok: bool = want_channel == "" or tl().get_sync_status().get("channel") == want_channel
		if expect_missing == "":
			if (want_other and other_skinned() and chan_ok) or (not want_other and t >= 14.0 and chan_ok): ok = true; break
		elif expect_missing in lines and chan_ok and other_skinned() == want_other:
			ok = true; break
	var sync: Dictionary = tl().get_sync_status() if tl() else {}
	if expect_missing != "":
		check(ok, "%s shows '%s' (lines %s)" % [mode, expect_missing, tl().get_missing_lines() if tl() else []])
	else:
		check(ok and (tl().get_missing_lines() as PackedStringArray).is_empty(), "%s: no missing lines" % mode)
	check(sync.get("network") == "on", "%s: sync network on (channel %s)" % [mode, sync.get("channel")])
	if want_channel != "": check(sync.get("channel") == want_channel, "%s: channel %s" % [mode, want_channel])
	var skin_on_other := other_skinned()
	check(skin_on_other == want_other, "%s: other player's skin applied=%s (expected %s)" % [mode, skin_on_other, want_other])
	if lobby == "private_then_public":


		set_lobby("custom", 2)
		var took := await switch_frames(func(): return true)
		var sy: Dictionary = tl().get_sync_status()
		check(took <= 1 and sy.get("network") == "off" and (tl().get_missing_lines() as PackedStringArray).is_empty() and not other_skinned(),
			"%s: private -> public: sync off, no missing lines, remote skins dropped in %d frame(s) (local_only: %s)" % [mode, took, sy.get("local_only", "")])
		check(tl().get_override_count() >= 1 and tl().get_swapped_count() >= 1, "%s: own skin + global pack still applied locally in public" % mode)
		var rx0: int = jv_rx[0]
		await wait(4.0)
		check(jv_rx[0] == rx0, "%s: 0 jv packets received after going public (got %d)" % [mode, jv_rx[0] - rx0])
	await wait(6.0 if not host else 1.0)
	finish()







func _content_items(st: Dictionary, peer: String) -> Dictionary:
	var out := {}
	for it in st.get("peers", {}).get(peer, []): out[it["id"]] = it
	return out
func _find_file(dir: String, name: String, depth := 0) -> bool:
	if depth > 6 or not DirAccess.dir_exists_absolute(dir): return false
	if FileAccess.file_exists(dir.path_join(name)): return true
	for d in DirAccess.get_directories_at(dir): if _find_file(dir.path_join(d), name, depth + 1): return true
	return false
func _net_content() -> void:
	var host := mode == "nethost"
	var peer := ENetMultiplayerPeer.new()
	var limit := float(OS.get_environment("JV_NET_WAIT")) if OS.get_environment("JV_NET_WAIT") != "" else 25.0
	var t0 := Time.get_ticks_msec()
	var el := func() -> float: return (Time.get_ticks_msec() - t0) / 1000.0
	var host_rig: ModelRig = null
	var body: MeshInstance3D = $Other/Body
	if host:
		peer.create_server(PORT, 4)
		Net.players = {1: {"name": "Hosty"}}
		multiplayer.peer_connected.connect(func(id): Net.players[id] = {"name": "Clienty"}; _peer_players[id] = $Other)
	else:
		peer.create_client("127.0.0.1", PORT)

		host_rig = mk_rigged("HostPlayer", "bot")
		var hp: Node = host_rig.get_parent().get_parent()
		body.get_parent().remove_child(body); hp.add_child(body)
		_peer_players[1] = hp
	multiplayer.multiplayer_peer = peer
	Net.active = true; Net.party_lan = true; Net.is_host = host
	_peer_players[multiplayer.get_unique_id()] = $Hitter
	if host:
		while el.call() < limit and int(tl().get_sync_status().get("modded_peers", 0)) == 0: await wait(0.25)
		await wait(1.0)
		var st: Dictionary = tl().get_content_status()
		var mine := {}
		for it in st["offer"]: mine[it["id"]] = it["kind"]
		check(mine.get("C001") == "outfit" and mine.get("C0B1") == "ball" and mine.get("B0D1") == "model" and mine.has("C0DE") and mine.has("C0FF") and mine.has("C0EE"), "nethost content: offer = outfits + ball + model (%s)" % [mine])
		check(int(st["offers_sent"]) >= 1 and st["sharing"] == "on", "nethost content: offer sent to the modded client (%d)" % int(st["offers_sent"]))
		var cl := _content_items(st, str(multiplayer.get_peers()[0]) if not multiplayer.get_peers().is_empty() else "?")
		check(cl.has("CAFE") and cl["CAFE"].get("state") == "have", "nethost content: client's offer received, CAFE already installed (%s)" % [cl])

		while el.call() < limit + 10.0 and el.call() < 14.0: await wait(0.5)
		var sent0 := int(tl().get_content_status()["offers_sent"])
		var f := FileAccess.open(OS.get_environment("JV_MODS_DIR").path_join("resources/packs/equipped.json"), FileAccess.WRITE)
		f.store_string('{"equipped":["C001","C0DE","C0FF","C0EE","C002"]}'); f.close()
		tl().rescan_picker_packs()
		await wait(1.2)
		var st2: Dictionary = tl().get_content_status()
		var ids2 := []
		for it in st2["offer"]: ids2.append(it["id"])
		check("C002" in ids2 and int(st2["offers_sent"]) > sent0, "nethost content: equipped C002 mid-lobby -> offer re-sent (%d -> %d)" % [sent0, int(st2["offers_sent"])])
		var t1: float = el.call()
		while el.call() - t1 < 30.0 and not multiplayer.get_peers().is_empty(): await wait(0.5)
		finish(); return

	var want := ["C001", "C0B1", "B0D1"]
	var done := false
	while el.call() < limit + 10.0:
		await wait(0.5)
		var st: Dictionary = tl().get_content_status()
		var dl: Dictionary = st.get("downloads", {})
		var all_in := true
		for w in want: all_in = all_in and dl.get(w) == "installed"
		if all_in and str(dl.get("C0FF", "")).begins_with("rejected") and str(dl.get("C0EE", "")).begins_with("rejected"): done = true; break
	var cs: Dictionary = tl().get_content_status()
	var hostc := _content_items(cs, "1")
	var dls: Dictionary = cs.get("downloads", {})
	check(done, "netclient content: C001 / C0B1 / B0D1 downloaded, C0FF / C0EE refused (%s)" % [dls])
	check(cs["catalog"]["state"] == "ready" and int(cs["catalog"]["items"]) == 7, "netclient content: mock repo manifest read (%s)" % [cs["catalog"]])
	check(str(hostc.get("C0DE", {}).get("state", "")).begins_with("hash mismatch") and not dls.has("C0DE"), "netclient content: C0DE hash mismatch -> never downloaded (%s)" % [hostc.get("C0DE")])
	check(str(dls.get("C0FF", "")).contains("hash mismatch"), "netclient content: tampered pack.zip rejected (%s)" % dls.get("C0FF"))
	check(str(dls.get("C0EE", "")).contains("unsafe entry"), "netclient content: zip-slip entry rejected (%s)" % dls.get("C0EE"))
	var cache := OS.get_environment("JV_MODS_DIR").path_join("resources/cache")
	var dirs := Array(DirAccess.get_directories_at(cache)) if DirAccess.dir_exists_absolute(cache) else []
	dirs = dirs.filter(func(d): return not String(d).begins_with("."))
	dirs.sort()
	check(dirs == ["b0d1_bean", "c001_red", "c0b1_ball"], "netclient content: cache holds exactly the 3 verified packs (%s)" % [dirs])
	check(not _find_file(OS.get_environment("JV_MODS_DIR").get_base_dir(), "evil.png"), "netclient content: nothing written outside the cache (no evil.png)")
	check(Array(cs.get("cached", [])).has("C001") and Array(cs.get("cached", [])).has("C0B1"), "netclient content: textures reloaded live with the cached packs (%s)" % [cs.get("cached")])
	await wait(1.0)
	var skinned := false
	for i in body.get_surface_override_material_count(): skinned = skinned or body.get_surface_override_material(i) != null
	check(skinned, "netclient content: host's downloaded outfit C001 rendered on the host's player")
	check(tl().get_content_status()["shown_ball"] == "C0B1", "netclient content: the host's ball C0B1 is shown (%s)" % tl().get_content_status()["shown_ball"])
	var t2: float = el.call()
	while el.call() - t2 < 6.0 and grafts(host_rig).is_empty(): await wait(0.25)
	var rep := ""
	for r in ml().get_rig_report(): if r.get("remote", "") != "": rep = r["remote"]
	check(not grafts(host_rig).is_empty() and rep == "B0D1", "netclient content: host's model B0D1 grafted on the host's rig (%d parts, remote %s, %s, rigs %s, model %s)" % [grafts(host_rig).size(), rep, ml().get_remote_models(), ml().get_rig_report(), mstatus().get("model")])

	var got := false
	var t3: float = el.call()
	while el.call() - t3 < 25.0:
		await wait(0.5)
		var s3: Dictionary = tl().get_content_status()
		if _content_items(s3, "1").has("C002") and s3.get("downloads", {}).get("C002") == "installed": got = true; break
	var ids: Array = tl().get_sync_status().get("players", {}).get("1", {}).get("ids", [])
	check(got and "C002" in ids, "netclient content: host's mid-lobby change re-broadcast -> C002 fetched and worn (%s, ids %s)" % [tl().get_content_status().get("downloads"), ids])
	check(int(tl().get_content_status()["rejected"]) == 2, "netclient content: 2 downloads rejected in total (%d)" % int(tl().get_content_status()["rejected"]))
	await wait(1.0)
	finish()


func _net_public(lobby: String) -> void:
	var host := mode == "nethost"
	var peer := ENetMultiplayerPeer.new()
	if host:
		peer.create_server(PORT, 4)
		Net.players = {1: {"name": "Hosty"}}
		multiplayer.peer_connected.connect(func(id): Net.players[id] = {"name": "Clienty"}; _peer_players[id] = $Other)
	else:
		peer.create_client("127.0.0.1", PORT)
	multiplayer.multiplayer_peer = peer
	(multiplayer as SceneMultiplayer).peer_packet.connect(func(_id, p: PackedByteArray): if p.size() >= 4 and p.slice(0, 4).get_string_from_ascii() == "JVMS": jv_rx[0] += 1)
	Net.active = true; Net.custom_lobby = true; Net.is_host = host
	_peer_players[multiplayer.get_unique_id()] = $Hitter
	if not host: _peer_players[1] = $Other
	var t0 := Time.get_ticks_msec()
	var connected_at := -1
	var limit := int(float(OS.get_environment("JV_NET_WAIT") if OS.get_environment("JV_NET_WAIT") != "" else "25") * 1000)

	while Time.get_ticks_msec() - t0 < limit:
		await wait(0.5)
		if connected_at < 0 and not multiplayer.get_peers().is_empty(): connected_at = Time.get_ticks_msec()
		if connected_at >= 0 and Time.get_ticks_msec() - connected_at > 14000: break
	check(connected_at >= 0, "%s (%s): peers connected" % [mode, lobby])
	var sy: Dictionary = tl().get_sync_status()
	check(jv_rx[0] == 0, "%s (%s): 0 jv packets received (got %d)" % [mode, lobby, jv_rx[0]])
	check(sy.get("network") == "off" and sy.get("channel") == "none" and str(sy.get("local_only", "")) != "", "%s (%s): texture network off, local only (%s)" % [mode, lobby, sy.get("local_only", "")])
	check((tl().get_missing_lines() as PackedStringArray).is_empty(), "%s (%s): no missing lines" % [mode, lobby])
	check(gate().is_cosmetic_active() and not gate().is_active() and full_off(), "%s (%s): textures on, shots/goofy off" % [mode, lobby])
	check(tl().get_override_count() >= 1 and tl().get_swapped_count() >= 1 and not other_skinned(), "%s (%s): own skin + global pack applied locally, nothing for the other player" % [mode, lobby])
	var st := status()
	check(st.get("texture_sync", {}).get("network") == "off" and st.get("libraries", {}).get("textures", {}).get("state") == "active", "%s (%s): status.json textures active, sync off" % [mode, lobby])
	await wait(4.0 if not host else 1.0)
	finish()





var jvgf_rx := [0]
const _GOOFY_HOST := '{"time_scale": 1.5, "player_scale": 1.5, "ball_params": {"ball.gravity": 30.0, "ball.radius": 0.4, "flight.air_drag": 0.3, "ball.floor_bounce": 0.9, "ball.curve_strength": 9.0, "ball.outline_home": [0, 1, 0, 1]}, "player_params": {"player.move_speed": 8.5, "player.jump_min_velocity": 9.0}}'
const _GOOFY_CLIENT := '{"ball_params": {"ball.gravity": 3.0, "ball.outline_home": [1, 0, 0, 1]}, "player_params": {"player.move_speed": 2.0}}'
func _goofy_write(vals: String) -> void:
	var f := FileAccess.open(OS.get_environment("JV_MODS_DIR").path_join("goofy").path_join("zz-sync.json"), FileAccess.WRITE)
	f.store_string(vals); f.close()
	goofy().reload_config()

func _net_goofy(lobby: String) -> void:
	var host := mode == "nethost"
	var g := goofy()
	var ph = load("res://game/ball/ball_physics.tres")
	var feel = load("res://game/player/player_feel.tres")
	var public := lobby in ["public", "quick", "browser"]
	_goofy_write(_GOOFY_HOST if host else _GOOFY_CLIENT)
	await wait(0.6)
	var ts_own := Engine.time_scale
	var peer := ENetMultiplayerPeer.new()
	if host: peer.create_server(PORT, 4)
	else: peer.create_client("127.0.0.1", PORT)
	multiplayer.multiplayer_peer = peer
	(multiplayer as SceneMultiplayer).peer_packet.connect(func(_id, p: PackedByteArray): if p.size() >= 4 and p.slice(0, 4).get_string_from_ascii() in ["JVGF", "JVGH"]: jvgf_rx[0] += 1)
	Net.active = true; Net.party_lan = lobby == "lan"; Net.custom_lobby = lobby != "lan"; Net.is_host = host; Net.in_match = false
	var limit := float(OS.get_environment("JV_NET_WAIT")) if OS.get_environment("JV_NET_WAIT") != "" else 25.0
	var t0 := Time.get_ticks_msec()
	var el := func() -> float: return (Time.get_ticks_msec() - t0) / 1000.0
	var connected := false
	if public:
		while el.call() < limit:
			await wait(0.5)
			if not multiplayer.get_peers().is_empty(): connected = true
			if connected and el.call() > 6.0 and not Net.in_match: Net.in_match = true
			if connected and el.call() > 12.0: break
		var st: Dictionary = g.get_sync_status()
		check(connected, "%s goofy (%s): peers connected" % [mode, lobby])
		check(jvgf_rx[0] == 0 and int(st["sent"]) == 0 and st["role"] == "off", "%s goofy (%s): no goofy sync packets, also in a match (rx %d, sent %d, role %s)" % [mode, lobby, jvgf_rx[0], st["sent"], st["role"]])
		check(not g.is_physics_allowed() and is_equal_approx(ph.gravity, 11.0) and is_equal_approx(feel.move_speed, 5.9) and is_equal_approx(Engine.time_scale, 1.0), "%s goofy (%s): local only, Goofy physics off, time scale 1 (g %.2f ts %.2f)" % [mode, lobby, ph.gravity, Engine.time_scale])
		Net.in_match = false
		await wait(3.0 if not host else 1.0)
		finish(); return
	if host:
		while el.call() < limit and int(g.get_sync_status()["peers"]) == 0: await wait(0.5)
		check(int(g.get_sync_status()["peers"]) == 1 and g.get_sync_status()["role"] == "host", "nethost goofy (%s): modded client said hello (%s)" % [lobby, g.get_sync_status()])
		check(is_equal_approx(ph.gravity, 30.0) and is_equal_approx(feel.move_speed, 8.5) and is_equal_approx(Engine.time_scale, 1.5) and ph.outline_home.is_equal_approx(Color(0, 1, 0, 1)), "nethost goofy (%s): host plays its own values" % lobby)
		await wait(2.0)
		var sent0 := int(g.get_sync_status()["sent"])
		Net.in_match = true
		await wait(0.5)
		check(int(g.get_sync_status()["sent"]) > sent0 and g.get_sync_status()["live"], "nethost goofy (%s): match start broadcast (sent %d -> %d)" % [lobby, sent0, g.get_sync_status()["sent"]])
		await wait(4.0)
		_goofy_write(_GOOFY_HOST.replace('"ball.gravity": 30.0', '"ball.gravity": 25.0'))
		await wait(0.8)
		check(is_equal_approx(ph.gravity, 25.0) and int(g.get_sync_status()["sent"]) >= sent0 + 2, "nethost goofy (%s): host changed gravity -> 25 mid-match, re-published (sent %d)" % [lobby, g.get_sync_status()["sent"]])
		var t1: float = el.call()
		while el.call() - t1 < 15.0 and not multiplayer.get_peers().is_empty(): await wait(0.5)
		Net.in_match = false
		finish(); return

	while el.call() < limit and int(g.get_sync_status()["received"]) == 0: await wait(0.25)
	await wait(0.5)
	var st0: Dictionary = g.get_sync_status()
	check(int(st0["received"]) >= 1 and st0["role"] == "client" and not st0["live"] and is_equal_approx(ph.gravity, 3.0) and is_equal_approx(feel.move_speed, 2.0), "netclient goofy (%s): host values received in the lobby, own values until the match starts (g %.2f, %s)" % [lobby, ph.gravity, st0])
	Net.in_match = true
	var got := false
	var t1: float = el.call()
	while el.call() - t1 < limit:
		await wait(0.25)
		if is_equal_approx(ph.gravity, 30.0) and is_equal_approx(feel.move_speed, 8.5): got = true; break
	var st2: Dictionary = g.get_sync_status()
	check(got and st2["authoritative"] == "host" and st2["live"], "netclient goofy (%s): match start -> host gravity + movement (g %.2f move %.2f, %s)" % [lobby, ph.gravity, feel.move_speed, st2])
	check(is_equal_approx(ph.radius, 0.4) and is_equal_approx(float(g.get_param("flight.air_drag")), 0.3) and is_equal_approx(ph.floor_bounce, 0.9) and is_equal_approx(ph.curve_strength, 9.0) and is_equal_approx(feel.jump_min_velocity, 9.0) and is_equal_approx(float(g.get_param("player.body_scale")), 1.5), "netclient goofy (%s): host hitbox, drag, bounce, spin, jump, body scale (r %.2f drag %s bounce %.2f curve %.2f jump %.2f)" % [lobby, ph.radius, g.get_param("flight.air_drag"), ph.floor_bounce, ph.curve_strength, feel.jump_min_velocity])
	check(is_equal_approx(Engine.time_scale, 1.5), "netclient goofy (%s): host time scale 1.5 (%.2f)" % [lobby, Engine.time_scale])
	check(ph.outline_home.is_equal_approx(Color(1, 0, 0, 1)), "netclient goofy (%s): visual-only outline stays the client's own (%s)" % [lobby, ph.outline_home])
	got = false
	var t2: float = el.call()
	while el.call() - t2 < limit:
		await wait(0.25)
		if is_equal_approx(ph.gravity, 25.0): got = true; break
	check(got and int(g.get_sync_status()["rejected"]) == 0, "netclient goofy (%s): host's mid-match change followed (g %.2f, rejected %d)" % [lobby, ph.gravity, g.get_sync_status()["rejected"]])
	Net.in_match = false
	await wait(0.6)
	check(not g.get_sync_status()["live"] and is_equal_approx(ph.gravity, 3.0) and is_equal_approx(feel.move_speed, 2.0) and is_equal_approx(ph.radius, 0.26) and is_equal_approx(Engine.time_scale, ts_own), "netclient goofy (%s): match ended -> own values and time scale restored (g %.2f ts %.2f)" % [lobby, ph.gravity, Engine.time_scale])
	multiplayer.multiplayer_peer = null
	reset_menu(); Net.is_host = false; set_lobby("offline", -1)
	if lobby != "lan" and _gate_cfg_orig != "":
		var gf := FileAccess.open(OS.get_environment("JV_GATE_CONFIG"), FileAccess.WRITE); gf.store_string(_gate_cfg_orig); gf.close(); gate().reload_config()
	await wait(1.0)
	check(g.get_sync_status()["role"] == "offline" and is_equal_approx(ph.gravity, 3.0) and is_equal_approx(feel.move_speed, 2.0), "netclient goofy (%s): left the lobby -> own config (g %.2f)" % [lobby, ph.gravity])
	finish()


const PC_PATH := "res://game/player/player.gd"
func pc_script() -> Script: return load(PC_PATH)
func pc_val(n: String) -> float: return float(pc_script().get(n))
func pc_all_stock() -> bool:
	var st: Dictionary = goofy().player_consts_get()["stock"]
	for n in st:
		var v = pc_script().get(n)
		if typeof(v) != typeof(st[n]) and not (typeof(v) == TYPE_INT and n == "JUMP_SPIKE_GRACE_US"): return false
		if float(v) != float(st[n]): return false
	return st.size() == 34
func pc_write_cfg(vals: Dictionary) -> void: _goofy_write(JSON.stringify({"player_consts": vals}))
func pc_match(on: bool) -> void:
	_rally = Rally.new() if on else null
	await wait(0.4)

func _pconsts() -> void:
	var g := goofy()
	set_lobby("offline", -1); await wait(0.9)
	var st: Dictionary = g.player_consts_get()
	check(st["available"] and st["reason"] == "ok", "pconsts: available (%s)" % st["reason"])
	check(pc_all_stock() and not g.player_consts_locked(), "pconsts: lobby -> all 34 stock, unlocked")

	pc_write_cfg({"BLOCK_TURN": 0.5, "SIDE_DIVE_DEG": 120.0, "JUMP_SPIKE_GRACE_US": 99999.6})
	await wait(0.3)
	check(g.player_consts_set("SET_DUMP_RISE_TAP", 5.0)["ok"], "pconsts: lobby set via Goofy API accepted")
	var a: Object = Engine.get_singleton("JVModAPI") if Engine.has_singleton("JVModAPI") else null
	check(a != null, "pconsts: CORE API present")
	var caps := PackedStringArray()
	for c in a.get_capabilities(): if c["status"] != "planned": caps.append(c["id"])
	a.register_mod("pc_test", "1.2", caps)
	check(a.get_capabilities().any(func(c): return c["id"] == "player.consts" and c["status"] == "impl"), "pconsts: capability player.consts is IMPL")
	check(a.player_consts_set("pc_test", "SPIKE_RECOVER", 1.5)["ok"], "pconsts: lobby set via CORE API accepted")
	st = g.player_consts_get()
	check(is_equal_approx(float(st["saved"]["SIDE_DIVE_DEG"]), 89.0) and int(st["saved"]["JUMP_SPIKE_GRACE_US"]) == 100000, "pconsts: clamped (89) + int rounded (%s)" % st["saved"])
	check(pc_all_stock(), "pconsts: lobby edits are saved, not live (script still stock)")

	await pc_match(true)
	check(g.player_consts_locked() and a.player_consts_locked("pc_test"), "pconsts: match -> locked (UI greys out)")
	check(pc_val("BLOCK_TURN") == 0.5 and pc_val("SET_DUMP_RISE_TAP") == 5.0 and pc_val("SPIKE_RECOVER") == 1.5 and pc_val("SIDE_DIVE_DEG") == 89.0 and int(pc_script().get("JUMP_SPIKE_GRACE_US")) == 100000,
		"pconsts: start-of-match set live (BLOCK_TURN %.2f)" % pc_val("BLOCK_TURN"))

	var r1: Dictionary = g.player_consts_set("BLOCK_TURN", 3.0)
	var r2: Dictionary = a.player_consts_set("pc_test", "BLOCK_TURN", 3.0)
	var r3: Dictionary = a.player_consts_reset("pc_test", "*")
	check(not r1["ok"] and "locked" in str(r1["error"]), "pconsts: mid-match Goofy API write rejected (%s)" % r1["error"])
	check(not r2["ok"] and "locked" in str(r2["error"]), "pconsts: mid-match CORE API write rejected (%s)" % r2["error"])
	check(not r3["ok"], "pconsts: mid-match reset rejected")
	pc_write_cfg({"BLOCK_TURN": 4.0}); await wait(0.5)
	check(pc_val("BLOCK_TURN") == 0.5 and pc_val("SPIKE_RECOVER") == 1.5, "pconsts: mid-match config reload ignored, values unchanged (%.2f)" % pc_val("BLOCK_TURN"))
	check(int(g.player_consts_get()["rejected"]) >= 4 and (g.player_consts_get()["warnings"] as Array).size() >= 4, "pconsts: rejections logged (%d)" % g.player_consts_get()["rejected"])

	await pc_match(false)
	check(pc_all_stock() and not g.player_consts_locked(), "pconsts: match end -> exact stock (all 34, ==) + unlocked")
	check(g.player_consts_set("BLOCK_TURN", 0.7)["ok"], "pconsts: lobby again -> edits accepted")

	pc_write_cfg({"BLOCK_TURN": 0.9}); await wait(0.3)
	await pc_match(true)
	check(pc_val("BLOCK_TURN") == 0.9, "pconsts: next match uses the new lobby set (%.2f)" % pc_val("BLOCK_TURN"))
	await pc_match(false)
	check(pc_all_stock(), "pconsts: second match end -> exact stock")

	pc_write_cfg({}); await wait(0.3)

func _net_pc(lobby: String) -> void:

	var host := mode == "nethost"
	var g := goofy()
	var expect := OS.get_environment("JV_PC_EXPECT")
	var want_notice := OS.get_environment("JV_PC_NOTICE")
	var not_notice := OS.get_environment("JV_PC_NOT_NOTICE")
	var unavailable := OS.get_environment("JV_PC_CONST") == "1"
	if host: pc_write_cfg({"BLOCK_TURN": 0.5, "SPIKE_RECOVER": 1.5})
	else: pc_write_cfg({"BLOCK_TURN": 7.0})
	await wait(0.6)
	var peer := ENetMultiplayerPeer.new()
	var n := [0]
	if host:
		peer.create_server(PORT, 4)
		Net.players = {1: {"name": "Hosty"}}
		multiplayer.peer_connected.connect(func(id): n[0] += 1; Net.players[id] = {"name": "Clienty%d" % n[0]})
	else: peer.create_client("127.0.0.1", PORT)
	multiplayer.multiplayer_peer = peer
	Net.active = true; Net.party_lan = lobby == "lan"; Net.custom_lobby = lobby != "lan"; Net.is_host = host
	var limit := float(OS.get_environment("JV_NET_WAIT")) if OS.get_environment("JV_NET_WAIT") != "" else 25.0
	var t0 := Time.get_ticks_msec()
	var el := func() -> float: return (Time.get_ticks_msec() - t0) / 1000.0
	if host:
		var want_peers := int(OS.get_environment("JV_PC_PEERS")) if OS.get_environment("JV_PC_PEERS") != "" else 1
		var want_hello := int(OS.get_environment("JV_PC_HELLOS")) if OS.get_environment("JV_PC_HELLOS") != "" else 0
		while el.call() < limit and (multiplayer.get_peers().size() < want_peers or int(g.get_sync_status()["peers"]) < want_hello): await wait(0.25)
		await wait(2.0)
		check(multiplayer.get_peers().size() == want_peers and int(g.get_sync_status()["peers"]) == want_hello, "nethost pc: %d peer(s), %d modded hello(s) (%s)" % [want_peers, want_hello, g.get_sync_status()])
		check(pc_all_stock(), "nethost pc: lobby -> stock")
		await pc_match(true)
		var st: Dictionary = g.player_consts_get()
		if expect == "apply":
			check(pc_val("BLOCK_TURN") == 0.5 and pc_val("SPIKE_RECOVER") == 1.5 and st["applied"] and str(st["notice"]) == "", "nethost pc: everyone has it -> host's set live (BLOCK_TURN %.2f, notice '%s')" % [pc_val("BLOCK_TURN"), st["notice"]])
		else:
			check(pc_all_stock() and not st["applied"], "nethost pc: someone lacks it -> host plays STOCK too (BLOCK_TURN %.2f)" % pc_val("BLOCK_TURN"))
			var ok := "OFF this match" in str(st["notice"])
			for w in want_notice.split("|", false): ok = ok and w in str(st["notice"])
			for w in not_notice.split("|", false): ok = ok and not (w in str(st["notice"]))
			check(ok, "nethost pc: host notice: %s" % st["notice"])
		var r: Dictionary = g.player_consts_set("BLOCK_TURN", 3.0)
		check(not r["ok"] and (pc_val("BLOCK_TURN") == (0.5 if expect == "apply" else 2.0)), "nethost pc: mid-match write rejected, value unchanged (%s)" % r["error"])
		check(int(g.get_sync_status()["sent"]) >= 0, "nethost pc: plan sent")
		await wait(6.0)
		await pc_match(false)
		check(pc_all_stock() and not g.player_consts_locked(), "nethost pc: match end -> exact stock, unlocked")
		var t1: float = el.call()
		while el.call() - t1 < 15.0 and not multiplayer.get_peers().is_empty(): await wait(0.5)
		finish(); return

	while el.call() < limit and multiplayer.get_peers().is_empty(): await wait(0.25)
	await wait(1.5)
	await pc_match(true)
	var got := false
	var st2: Dictionary = {}
	while el.call() < limit:
		st2 = g.player_consts_get()
		if unavailable or st2["plan_received"]: got = true; break
		await wait(0.25)
	if unavailable:
		check(not st2["available"] and pc_val("BLOCK_TURN") == 2.0, "netclient pc: player.consts unavailable here -> stock (%s)" % st2["reason"])
	elif expect == "apply":
		check(got and pc_val("BLOCK_TURN") == 0.5 and pc_val("SPIKE_RECOVER") == 1.5 and st2["applied"], "netclient pc: host's start-of-match set reached the client, own edit 7.0 ignored (BLOCK_TURN %.2f)" % pc_val("BLOCK_TURN"))
	else:
		check(got and pc_all_stock() and not st2["applied"], "netclient pc: host's plan says stock -> stock (BLOCK_TURN %.2f, plan %s)" % [pc_val("BLOCK_TURN"), got])
	if not unavailable:
		var r2: Dictionary = g.player_consts_set("BLOCK_TURN", 9.0)
		check(not r2["ok"] and pc_val("BLOCK_TURN") == (0.5 if expect == "apply" else 2.0), "netclient pc: mid-match write rejected (%s)" % r2["error"])
	await wait(2.0)
	await pc_match(false)
	check(pc_all_stock(), "netclient pc: match end -> exact stock")
	await wait(1.0)
	finish()

func other_skinned() -> bool:
	var m := $Other/Body as MeshInstance3D
	for i in m.get_surface_override_material_count():
		if m.get_surface_override_material(i) != null: return true
	return false

func _plain(host: bool) -> void:

	var peer := ENetMultiplayerPeer.new()
	if host: peer.create_server(PORT, 4)
	else: peer.create_client("127.0.0.1", PORT)
	multiplayer.multiplayer_peer = peer
	var got := [0]
	(multiplayer as SceneMultiplayer).peer_packet.connect(func(_id, _p): got[0] += 1)


	var t0 := Time.get_ticks_msec()
	var limit := int(float(OS.get_environment("JV_NET_WAIT") if OS.get_environment("JV_NET_WAIT") != "" else "25") * 1000) + (5000 if host else -7000)
	var seen := -1
	while Time.get_ticks_msec() - t0 < limit:
		await wait(0.5)
		if seen < 0 and not multiplayer.get_peers().is_empty(): seen = Time.get_ticks_msec()
		if seen >= 0 and Time.get_ticks_msec() - seen > 20000: break
	print("PLAIN %s ok; peers=%s raw_packets_seen=%d" % ["host" if host else "client", multiplayer.get_peers(), got[0]])
	if host: print("RESULT 1 passed, 0 failed")
	else:
		var ok: bool = got[0] == 0 and get_node_or_null("/root/JVModGate") == null
		print(("PASS" if ok else "FAIL") + " unmodded client received %d jv packets" % got[0])
		print("RESULT %d passed, %d failed" % [1 if ok else 0, 0 if ok else 1])
	get_tree().quit(0)





func _plot_line(img: Image, a: Vector2, b: Vector2, col: Color) -> void:
	var d := b - a
	var n := int(ceili(d.length())) + 1
	for i in n + 1:
		var p: Vector2 = a.lerp(b, float(i) / float(maxi(n, 1)))
		var x := int(round(p.x)); var y := int(round(p.y))
		if x >= 0 and y >= 0 and x < img.get_width() and y < img.get_height():
			img.set_pixel(x, y, col)

func _plot_mark(img: Image, c: Vector2, col: Color, question: bool) -> void:

	var marks: Array = []
	if question:
		for x in range(-6, 7): marks.append(Vector2(x, -16)); marks.append(Vector2(x, -15))
		for y in range(-16, -4): marks.append(Vector2(6, y))
		for x in range(-2, 7): marks.append(Vector2(x, -4))
		for y in range(-4, 2): marks.append(Vector2(-2, y))
		for y in range(6, 11):
			for x in range(-3, 4): marks.append(Vector2(x, y))
	else:
		for x in range(-3, 4):
			for y in range(-3, 4): marks.append(Vector2(x, y))
	for m in marks:
		var x := int(round(c.x + m.x)); var y := int(round(c.y + m.y))
		if x >= 0 and y >= 0 and x < img.get_width() and y < img.get_height():
			img.set_pixel(x, y, col)

func save_spin_shot(tag: String, sd: Dictionary) -> String:
	var dir := OS.get_environment("JV_CURVE_SHOTS")
	if dir == "": dir = OS.get_user_data_dir().path_join("curve-display-shots")
	DirAccess.make_dir_recursive_absolute(dir)
	var img := Image.create(720, 720, false, Image.FORMAT_RGBA8)
	img.fill(Color(0.10, 0.16, 0.10, 1))

	var origin := Vector2(360, 360)
	var sc := 28.0
	var to2 := func(p: Vector3) -> Vector2: return origin + Vector2(p.x, p.z) * sc
	_plot_line(img, to2.call(Vector3(-4.5, 0, -9)), to2.call(Vector3(4.5, 0, -9)), Color(0.25, 0.4, 0.25))
	_plot_line(img, to2.call(Vector3(4.5, 0, -9)), to2.call(Vector3(4.5, 0, 9)), Color(0.25, 0.4, 0.25))
	_plot_line(img, to2.call(Vector3(4.5, 0, 9)), to2.call(Vector3(-4.5, 0, 9)), Color(0.25, 0.4, 0.25))
	_plot_line(img, to2.call(Vector3(-4.5, 0, 9)), to2.call(Vector3(-4.5, 0, -9)), Color(0.25, 0.4, 0.25))
	_plot_line(img, to2.call(Vector3(-4.5, 0, 0)), to2.call(Vector3(4.5, 0, 0)), Color(0.35, 0.35, 0.45))
	var full: bool = str(sd.get("style", "")) == "full"
	var col := Color(0.35, 0.95, 1.0, clampf(float(sd.get("fade", 1)), 0.15, 1.0)) if full else Color(0.62, 0.62, 0.62, 0.45)
	var pts: Array = sd.get("points", []) if bool(sd.get("path", false)) else []
	for i in range(maxi(pts.size() - 1, 0)):
		_plot_line(img, to2.call(pts[i]), to2.call(pts[i + 1]), col)
		_plot_line(img, to2.call(pts[i]) + Vector2(1, 0), to2.call(pts[i + 1]) + Vector2(1, 0), col)
	if bool(sd.get("question", false)):
		var qcol := Color(1, 1, 1, 0.7) if full else Color(0.7, 0.7, 0.7, 0.45)
		_plot_mark(img, to2.call(sd.get("question_pos", Vector3.ZERO)), qcol, true)
	var path := dir.path_join(tag + ".png")
	img.save_png(path)
	print("screenshot=" + path + " style=%s question=%s points=%d" % [sd.get("style"), sd.get("question"), pts.size()])
	return path

func _curve_send(d: Dictionary) -> void:
	var payload := JSON.stringify(d).to_utf8_buffer()
	for id in multiplayer.get_peers():
		(multiplayer as SceneMultiplayer).send_bytes(payload, id)

func _curve_apply(d: Dictionary, me: Player, other: Player) -> void:
	if gball == null: return
	var pos: Array = d.get("pos", [0, 2.5, 4])
	var vel: Array = d.get("vel", [0, 6, -8])
	gball.global_position = Vector3(float(pos[0]), float(pos[1]), float(pos[2]))
	gball.vel = Vector3(float(vel[0]), float(vel[1]), float(vel[2]))
	gball.curve = float(d.get("curve", 8))
	gball.strike_seq = int(d.get("seq", gball.strike_seq + 1))
	gball.float_serve = false
	gball.frozen = false
	var who := str(d.get("hitter", "host"))
	var mine := (mode == "curvehost" and who == "host") or (mode == "curveclient" and who == "client")
	gball.hit_by = me if mine else other
	gball.strike_source = gball.hit_by
	if d.has("aim"):
		var am: Array = d.get("aim", [0, 0, 0])
		var ap := Vector3(float(am[0]), float(am[1]), float(am[2]))
		(me if mine else other).aim_point = ap

func _curve_display_net() -> void:
	var host := mode == "curvehost"
	var peer := ENetMultiplayerPeer.new()
	if host:
		var err := peer.create_server(PORT, 4)
		check(err == OK, "curvehost: listen on %d (%s)" % [PORT, err])
		Net.players = {1: {"name": "Hosty"}}
		multiplayer.peer_connected.connect(func(id): Net.players[id] = {"name": "Clienty"})
	else:
		var err := peer.create_client("127.0.0.1", PORT)
		check(err == OK, "curveclient: connect 127.0.0.1:%d (%s)" % [PORT, err])
	multiplayer.multiplayer_peer = peer
	var inbox: Array = []
	(multiplayer as SceneMultiplayer).peer_packet.connect(func(_id, p: PackedByteArray):
		var txt := p.get_string_from_utf8()
		var v = JSON.parse_string(txt)
		if v is Dictionary: inbox.append(v)
	)
	Net.active = true; Net.custom_lobby = true; Net.is_host = host; Net.privacy = 2
	set_lobby("custom", 0)

	gball = Ball.new(); gball.name = "Ball"; add_child(gball)
	var me := mk_player("Player", 1)
	var other := mk_player("Other", -1)
	player = me
	var t0 := Time.get_ticks_msec()
	var limit := float(OS.get_environment("JV_NET_WAIT")) if OS.get_environment("JV_NET_WAIT") != "" else 25.0
	var joined := false
	while (Time.get_ticks_msec() - t0) / 1000.0 < limit:
		await wait(0.25)
		if shots() != null and gate() != null and gate().is_active() and (host and not multiplayer.get_peers().is_empty() or not host and multiplayer.get_unique_id() != 1 and multiplayer.get_peers().size() >= 0 and gball != null):

			if host and multiplayer.get_peers().size() >= 1: joined = true; break
			if not host and multiplayer.get_remote_sender_id() == 0 and peer.get_connection_status() == MultiplayerPeer.CONNECTION_CONNECTED:
				joined = true; break

	if not host:
		joined = peer.get_connection_status() == MultiplayerPeer.CONNECTION_CONNECTED
	check(joined and shots() != null and gate().is_active(), "%s: connected, shots on (peers %s, reason %s)" % [mode, multiplayer.get_peers(), gate().get_reason() if gate() else "?"])
	await wait(0.6)
	var sh := shots()

	if host:
		gball.global_position = Vector3(0, 2.5, 4)
		gball.vel = Vector3(1.2, 6.5, -8.5)
		gball.curve = 9.0
		gball.float_serve = false
		gball.frozen = false
		gball.strike_seq += 1
		gball.hit_by = me
		gball.strike_source = me
		me.aim_point = Vector3(0, 0, -5)
		_curve_send({"kind": "spin", "pos": [0, 2.5, 4], "vel": [1.2, 6.5, -8.5], "curve": 9.0, "seq": gball.strike_seq, "hitter": "host", "aim": [0, 0, -5]})
	else:
		var got := false
		var t1 := Time.get_ticks_msec()
		while (Time.get_ticks_msec() - t1) / 1000.0 < 12.0:
			if inbox.size() > 0:
				_curve_apply(inbox.pop_front(), me, other)
				got = true
				break
			await wait(0.1)
		check(got, "curveclient: received the host's spin state")
	for i in 8: await get_tree().physics_frame
	var sd: Dictionary = sh.get_spin_display()
	if host:
		check(bool(sd.get("on")) and bool(sd.get("local")) and str(sd.get("style")) == "full" and bool(sd.get("question")) and bool(sd.get("shimmer")),
			"curvehost: hitter sees the full question display (%s)" % str(sd))
		save_spin_shot("hitter-host-full", sd)
	else:
		check(bool(sd.get("on")) and sd.get("local") == false and str(sd.get("style")) == "grey" and bool(sd.get("question")) and not bool(sd.get("path")) and not bool(sd.get("path_visible", true)),
			"curveclient: other sees the landing box + ?, no path (%s)" % str(sd))
		save_spin_shot("remote-client-grey", sd)

	if host:
		var got2 := false
		var t2 := Time.get_ticks_msec()
		while (Time.get_ticks_msec() - t2) / 1000.0 < 12.0:
			if inbox.size() > 0:
				_curve_apply(inbox.pop_front(), me, other)
				got2 = true
				break
			await wait(0.1)
		check(got2, "curvehost: received the client's spin state")
	else:
		gball.global_position = Vector3(0.4, 2.4, -3.5)
		gball.vel = Vector3(-1.0, 6.2, 8.2)
		gball.curve = -8.5
		gball.float_serve = false
		gball.frozen = false
		gball.strike_seq += 1
		gball.hit_by = me
		gball.strike_source = me
		me.aim_point = Vector3(0, 0, 5)
		_curve_send({"kind": "spin", "pos": [0.4, 2.4, -3.5], "vel": [-1.0, 6.2, 8.2], "curve": -8.5, "seq": gball.strike_seq, "hitter": "client", "aim": [0, 0, 5]})
	for i in 8: await get_tree().physics_frame
	var sd2: Dictionary = sh.get_spin_display()
	if host:
		check(bool(sd2.get("on")) and sd2.get("local") == false and str(sd2.get("style")) == "grey" and bool(sd2.get("question")) and not bool(sd2.get("path")) and not bool(sd2.get("path_visible", true)),
			"curvehost: host sees the landing box + ?, no path, when the client is the hitter (%s)" % str(sd2))
		save_spin_shot("remote-host-grey", sd2)
		await wait(2.0)
	else:
		check(bool(sd2.get("on")) and bool(sd2.get("local")) and str(sd2.get("style")) == "full" and bool(sd2.get("question")),
			"curveclient: hitter sees the full question display (%s)" % str(sd2))
		save_spin_shot("hitter-client-full", sd2)
		await wait(1.0)
	finish()


var gball: Ball = null
var net_bv := Vector3.ZERO
var game_vel := Vector3.ZERO
func shots() -> Node: return get_node_or_null("/root/JVExtraShots")
func goofy() -> Node: return get_node_or_null("/root/JVGoofyMods")
func _on_ball_struck(_p: Vector3, _pw: float, _k: int) -> void: net_bv = gball.vel
func _on_touched(kind: int, who: Player) -> void:
	if _rally != null: _rally.judge_touch(kind, who)
func _physics_process(_dt: float) -> void:
	if _rally != null: _rally.pre_tick()
func mk_player(n: String, side: int) -> Player:
	var p := Player.new(); p.name = n; p.team_side = side; p.ball = gball; add_child(p)
	p.touched.connect(_on_touched.bind(p))
	return p


func key(pk: Key, down: bool) -> void:
	var e := InputEventKey.new(); e.physical_keycode = pk; e.pressed = down
	Input.parse_input_event(e)
func arm(id: String) -> void:
	if not InputMap.has_action("jvm_shot_" + id):
		var e2 := InputEventKey.new(); e2.physical_keycode = KEY_KP_1; e2.pressed = true; Input.parse_input_event(e2)
		await frame(); await frame(); e2.pressed = false; Input.parse_input_event(e2); await frame(); return
	var ev: InputEventKey = InputMap.action_get_events("jvm_shot_" + id)[0]
	key(ev.physical_keycode, true); await frame(); await frame()
	key(ev.physical_keycode, false); await frame()
var _held: Array = []
func hold(actions: Array) -> void:
	for x in _held: Input.action_release(x)
	_held = actions
	for x in actions: Input.action_press(x)
func touch(p: Player, kind := 1, power := 0.7) -> bool:
	var ok := p.do_touch(kind, power)
	await get_tree().physics_frame; await get_tree().physics_frame
	return ok
func new_rally() -> void:
	_rally.touch_count = 0; _rally.touch_side = 0; _rally.last_toucher = null; _rally.fault = ""
	await get_tree().physics_frame; await get_tree().physics_frame

func _gameplay() -> void:
	reset_menu()
	var ph = load("res://game/ball/ball_physics.tres")
	var r0: float = ph.radius
	gball = Ball.new(); gball.name = "Ball"; add_child(gball)
	gball.struck.connect(_on_ball_struck)
	var me := mk_player("Player", 1); var mate := mk_player("Mate", 1); var foe := mk_player("Foe", -1)
	player = me
	await expect_gameplay()
	var sh := shots()
	var st: Dictionary = sh.get_status()
	check(st.get("backend") == "jv_ball" and int(st.get("balls_hooked", 0)) == 1, "shots: jv_ball backend hooked the game's Ball (%s)" % str(st))
	var own := InputMap.get_actions().filter(func(x): return str(x).begins_with("jvm_shot_"))
	check(own.is_empty(), "shots: no new standalone shot keys (%d jvm_shot_* actions)" % own.size())

	check(InputMap.action_get_events("move_forward").size() == 2 and not InputMap.has_action("jvm_forward"), "controls: move_forward keeps W + stick-up (no runtime unbind)")
	key(KEY_W, true); await frame(); await frame()
	check(Input.is_action_pressed("move_forward") and Input.get_vector("move_left", "move_right", "move_forward", "move_back").y < -0.9, "controls: W walks forward normally")
	key(KEY_W, false); await frame()

	var a0 := int(sh.get_applied_count())
	await touch(me)
	check(int(sh.get_applied_count()) == a0 and net_bv == gball.vel, "shots: plain touch leaves the game's launch alone")
	hold(["sprint"]); await touch(me); hold([])
	check(int(sh.get_applied_count()) == a0, "shots: sprinting into a touch (normal play) is not a shot")
	game_vel = gball.vel

	hold(["sprint", "strafe"]); await touch(me, 2); hold([])
	check(sh.get_last_event() == "applied high_shot" and int(sh.get_applied_count()) == a0 + 1, "combo: sprint+strafe + set touch = high_shot (%s)" % sh.get_last_event())
	check(net_bv.is_equal_approx(gball.vel), "shots: our launch runs before main.gd's struck handler (net bv %s == ball %s)" % [net_bv, gball.vel])
	check(gball.vel.z < 0.0 and gball.vel.y > 5.0, "shots: high_shot launched up toward the player's aim_point (%s)" % gball.vel)
	hold(["sprint", "strafe"]); await touch(me, 1); hold([])
	check(sh.get_last_event() == "applied low_shot", "combo: sprint+strafe + bump = low_shot")

	var ts0 := int(sh.get_applied_count())
	hold(["move_forward"]); await touch(me); hold([])
	check(int(sh.get_applied_count()) == ts0, "topspin: forward held on a GROUND touch = the game's normal shot (%s)" % sh.get_last_event())
	me.airborne = true; hold(["move_forward"]); await touch(me); hold([]); me.airborne = false
	check(sh.get_last_event() == "applied topspin" and gball.dive_g > 1.001, "topspin: airborne + forward held = topspin, Ball.dive_g %.2f" % gball.dive_g)
	me.airborne = true; await touch(me); me.airborne = false
	check(sh.get_last_event() == "applied topspin" and int(sh.get_applied_count()) == ts0 + 1, "topspin: airborne without forward = normal shot")

	var ax := InputEventJoypadMotion.new(); ax.axis = 1; ax.axis_value = -0.7; Input.parse_input_event(ax); await frame(); await frame()
	me.airborne = true; await touch(me); me.airborne = false
	ax.axis_value = 0.0; Input.parse_input_event(ax); await frame()
	check(int(sh.get_applied_count()) == ts0 + 2 and sh.get_last_event() == "applied topspin", "topspin: left stick up (controller) while airborne")

	var ki := InputEventKey.new(); ki.physical_keycode = KEY_I
	InputBinds.apply({"move_forward": [ki]}); await frame()
	key(KEY_I, true); await frame(); await frame()
	me.airborne = true; await touch(me); me.airborne = false
	key(KEY_I, false); await frame()
	check(int(sh.get_applied_count()) == ts0 + 3, "topspin: follows a rebind of move forward to I")
	InputBinds.apply(); await frame()

	hold(["move_forward", "move_back"]); me.airborne = true; await touch(me); me.airborne = false; hold([])
	check(int(sh.get_applied_count()) == ts0 + 3, "topspin: forward+back held = no forward component, no topspin")


	var bapi = Engine.get_singleton("JVModAPI") if Engine.has_singleton("JVModAPI") else null
	var bid := "goofy.topspin_spike_boost"
	var binfo: Dictionary = bapi.gameplay_info("goofy", bid) if bapi != null else {}
	check(not binfo.is_empty() and is_equal_approx(float(binfo.get("default", 0.0)), 1.12) and is_equal_approx(float(binfo.get("min", 0.0)), 1.0) and is_equal_approx(float(binfo.get("max", 0.0)), 1.3),
		"spike boost: Goofy registry setting %s, default 1.12, range 1.0..1.3 (%s)" % [bid, str(binfo)])
	var bpos0 := gball.global_position
	var bshot := func(kind: int, held: Array, air: bool) -> Dictionary:
		gball.global_position = bpos0; gball.vel = Vector3.ZERO; gball.curve = 0.0
		var n0 := int(sh.get_applied_count())
		hold(held); me.airborne = air; await touch(me, kind, 0.8); me.airborne = false; hold([])
		var lt: Dictionary = sh.get_last_topspin()
		return {"applied": int(sh.get_applied_count()) > n0, "ev": sh.get_last_event(), "ts": lt, "vel": gball.vel}

	var r_on: Dictionary = await bshot.call(3, ["move_forward"], true)
	var t_on: Dictionary = r_on["ts"]
	check(r_on["ev"] == "applied topspin" and String(t_on.get("spike_boost_note", "")) == "applied" and is_equal_approx(float(t_on.get("spike_boost", 0.0)), 1.12)
		and is_equal_approx(float(t_on["speed_k"]), float(t_on["speed_k_plain"]) * 1.12),
		"spike boost: topspin spike + air forward = x1.12 faster (cursor landing: real-game probe; %s k %.3f plain %.3f err %.3f)" % [t_on.get("spike_boost_note"), float(t_on.get("speed_k", 0)), float(t_on.get("speed_k_plain", 0)), float(t_on.get("err", 9))])
	var bset_ok := bapi != null and bool(bapi.gameplay_set("goofy", bid, 1.0).get("ok", false))
	var r_one: Dictionary = await bshot.call(3, ["move_forward"], true)
	var t_one: Dictionary = r_one["ts"]
	check(bset_ok and is_equal_approx(float(t_one.get("spike_boost", 0.0)), 1.0) and float(t_on["hspeed"]) > float(t_one["hspeed"]) * 1.10 and float(t_on["hspeed"]) < float(t_one["hspeed"]) * 1.14,
		"spike boost: setting 1.0 = no boost; boosted hspeed %.2f vs %.2f m/s (x%.3f)" % [float(t_on.get("hspeed", 0)), float(t_one.get("hspeed", 0)), float(t_on.get("hspeed", 0)) / maxf(0.001, float(t_one.get("hspeed", 0)))])
	if bapi != null: bapi.gameplay_set("goofy", bid, 1.25)
	var r_25: Dictionary = await bshot.call(3, ["move_forward"], true)
	check(is_equal_approx(float(r_25["ts"].get("spike_boost_wanted", 0.0)), 1.25), "spike boost: follows the setting (1.25 -> %s)" % str(r_25["ts"].get("spike_boost_wanted")))
	if bapi != null: bapi.gameplay_set("goofy", bid, 9.0)
	check(bapi != null and float(bapi.gameplay_get("goofy", bid)) <= 1.3 + 1e-6, "spike boost: never above 1.3 (asked 9.0, got %s)" % str(bapi.gameplay_get("goofy", bid) if bapi != null else null))
	check(bapi != null and bool(bapi.gameplay_reset("goofy", bid)) and is_equal_approx(float(bapi.gameplay_get("goofy", bid)), 1.12), "spike boost: reset to default = 1.12")


	gball.global_position = bpos0; gball.vel = Vector3.ZERO
	var n_dump := int(sh.get_applied_count())
	hold(["move_forward"]); me.airborne = true; await touch(me, 4, 0.8); me.airborne = false; hold([])
	check(int(sh.get_applied_count()) == n_dump and net_bv == gball.vel and sh.get_last_event().begins_with("kept the game's dump"),
		"dump: air + forward dump stays the game's own dump launch, topspin not applied (%s)" % sh.get_last_event())
	check(InputMap.action_get_events("move_forward").size() == 2, "dump: move_forward still bound (nothing unbound)")
	for kc in [[2, "set"], [1, "bump"]]:
		var rk: Dictionary = await bshot.call(kc[0], ["move_forward"], true)
		check(rk["ev"] == "applied topspin" and is_equal_approx(float(rk["ts"].get("spike_boost", 0.0)), 1.0) and is_equal_approx(float(rk["ts"]["speed_k"]), float(rk["ts"]["speed_k_plain"])),
			"spike boost: %s + air forward = topspin unboosted (%s)" % [kc[1], rk["ts"].get("spike_boost_note")])

	var tdefp := OS.get_environment("JV_MODS_DIR").path_join("shots").path_join("00-default.json")
	var tdef_txt := FileAccess.get_file_as_string(tdefp)
	var tdef: Dictionary = JSON.parse_string(tdef_txt)
	var tshots: Dictionary = tdef.get("shots", {})
	tshots["topspin"] = {"combo": {"hold": ["sprint", "strafe"], "air": "air", "touch": "not_set"}}
	tdef["shots"] = tshots
	var tw := FileAccess.open(tdefp, FileAccess.WRITE); tw.store_string(JSON.stringify(tdef)); tw.close(); sh.reload_config()
	var r_nf: Dictionary = await bshot.call(3, ["sprint", "strafe"], true)
	check(r_nf["ev"] == "applied topspin" and is_equal_approx(float(r_nf["ts"].get("spike_boost", 0.0)), 1.0) and is_equal_approx(float(r_nf["ts"]["speed_k"]), float(r_nf["ts"]["speed_k_plain"])),
		"spike boost: topspin spike WITHOUT forward = normal topspin (%s, %s)" % [r_nf["ev"], r_nf["ts"].get("spike_boost_note")])

	tshots["topspin"] = {"trigger": "off"}
	tdef["shots"] = tshots
	tw = FileAccess.open(tdefp, FileAccess.WRITE); tw.store_string(JSON.stringify(tdef)); tw.close(); sh.reload_config()
	gball.global_position = bpos0; gball.vel = Vector3.ZERO
	var n_reg := int(sh.get_applied_count())
	hold(["move_forward"]); me.airborne = true; await touch(me, 3, 0.8); me.airborne = false; hold([])
	check(int(sh.get_applied_count()) == n_reg and net_bv == gball.vel, "spike boost: regular spike + air forward = the game's own spike, unchanged (%s)" % sh.get_last_event())
	tw = FileAccess.open(tdefp, FileAccess.WRITE); tw.store_string(tdef_txt); tw.close(); sh.reload_config()
	var r_back: Dictionary = await bshot.call(3, ["move_forward"], true)
	check(r_back["ev"] == "applied topspin" and String(r_back["ts"].get("spike_boost_note", "")) == "applied", "spike boost: default config restored (%s)" % r_back["ts"].get("spike_boost_note"))

	me.serving = true; await get_tree().physics_frame; await get_tree().physics_frame
	hold(["move_forward"]); me.airborne = true; await touch(me, 1, 0.7); me.airborne = false; hold([])
	check(sh.get_last_event() != "applied topspin" or int(sh.get_applied_count()) == ts0 + 3, "topspin: not used on a serve (%s)" % sh.get_last_event())

	var serve_cases := [[["sprint"], 0.2, false, "high_serve"], [["sprint"], 0.7, false, "low_serve"], [["sprint"], 1.0, false, "fast_serve"],
		[["sprint"], 0.8, true, "jump_serve"], [["sprint", "move_back"], 0.8, false, "standing_float"], [["sprint", "move_back"], 0.8, true, "float_serve"]]
	for c in serve_cases:
		hold(c[0]); me.airborne = c[2]; await touch(me, 1, c[1]); hold([]); me.airborne = false
		check(sh.get_last_event() == "applied " + c[3], "combo: serve + %s power %.1f %s = %s (%s)" % ["+".join(c[0]), c[1], "air" if c[2] else "ground", c[3], sh.get_last_event()])
		if c[3].contains("float"): check(gball.float_serve and gball.float_seed != 0, "shots: %s uses the game's own float (seed %d)" % [c[3], gball.float_seed])
	var a_plain := int(sh.get_applied_count())
	await touch(me, 1, 0.7)
	check(int(sh.get_applied_count()) == a_plain, "shots: plain serve stays the game's")
	me.serving = false
	await wait(0.7)

	var fp0 := int(sh.get_applied_count())
	me.shared_ball_off = true
	me.serving = true; await get_tree().physics_frame; await get_tree().physics_frame; me.serving = false
	hold(["move_forward"]); me.airborne = true; await touch(me, 3, 0.8); me.airborne = false; hold([])
	check(sh.get_last_event() == "applied topspin" and int(sh.get_applied_count()) == fp0 + 1, "free play: airborne + forward right after an own-ball toss = topspin (%s)" % sh.get_last_event())
	check(is_equal_approx(float(sh.get_last_topspin().get("spike_boost", 0.0)), 1.0), "spike boost: own-ball serve-toss topspin in a match (serve contact) is NOT boosted (%s)" % sh.get_last_topspin().get("spike_boost_note"))
	me.serving = true; await get_tree().physics_frame; await get_tree().physics_frame; me.serving = false
	hold(["move_forward"]); await touch(me, 3, 0.8); hold([])
	check(int(sh.get_applied_count()) == fp0 + 1, "free play: forward on a GROUND own-ball hit = the game's shot (%s)" % sh.get_last_event())
	me.serving = true; await get_tree().physics_frame; await get_tree().physics_frame; me.serving = false
	var fcam := Camera3D.new(); add_child(fcam); fcam.current = true; fcam.global_transform = Transform3D(Basis(), Vector3(0, 3, 8))
	hold(["move_right"]); me.airborne = true; await touch(me, 3, 0.6); me.airborne = false; hold([])
	fcam.queue_free()
	check(sh.get_last_event() == "applied curveball" and int(sh.get_applied_count()) == fp0 + 2, "free play: airborne + right right after an own-ball toss = Curveball (%s)" % sh.get_last_event())
	me.serving = true; await get_tree().physics_frame; await get_tree().physics_frame; me.serving = false
	hold(["sprint"]); me.airborne = true; await touch(me, 1, 0.8); me.airborne = false; hold([])
	check(sh.get_last_event() == "applied jump_serve", "free play: sprint jump serve on an own ball stays a serve combo (%s)" % sh.get_last_event())
	me.shared_ball_off = false

	var sk0 := int(sh.get_skipped_count()); var fp1 := int(sh.get_applied_count())
	me.serving = true; await get_tree().physics_frame; await get_tree().physics_frame; me.serving = false
	hold(["move_forward"]); me.airborne = true; await touch(me, 3, 0.8); me.airborne = false; hold([])
	check(int(sh.get_applied_count()) == fp1 and int(sh.get_skipped_count()) == sk0 + 1 and sh.get_last_event().contains("serve contact"), "match: topspin inside the 0.6 s serve window is skipped and logged (%s)" % sh.get_last_event())

	_freeplay = true; await wait(0.5)
	check(gate().get_mode() == "Free Play", "gate mode Free Play (%s)" % gate().get_mode())
	var fp2 := int(sh.get_applied_count())
	me.serving = true; await get_tree().physics_frame; await get_tree().physics_frame; me.serving = false
	hold(["move_forward"]); me.airborne = true; await touch(me, 3, 0.8); me.airborne = false; hold([])
	check(sh.get_last_event() == "applied topspin" and int(sh.get_applied_count()) == fp2 + 1, "free play shared ball: topspin on your own serve toss fires (%s)" % sh.get_last_event())
	check(is_equal_approx(float(sh.get_last_topspin().get("spike_boost", 0.0)), 1.12), "spike boost: Free Play (solo court) own-toss topspin spike is played as a rally spike -> boosted (%s)" % sh.get_last_topspin().get("spike_boost_note"))
	me.serving = true; await get_tree().physics_frame; await get_tree().physics_frame; me.serving = false
	hold(["sprint"]); me.airborne = true; await touch(me, 1, 0.8); me.airborne = false; hold([])
	check(sh.get_last_event() == "applied jump_serve", "free play shared ball: sprint jump serve stays a serve combo (%s)" % sh.get_last_event())

	check(not sh.is_debug_visible(), "shot debugger off by default")
	var f7 := InputEventKey.new(); f7.keycode = KEY_F7; f7.pressed = true; Input.parse_input_event(f7)
	await get_tree().process_frame; await get_tree().process_frame
	check(sh.is_debug_visible(), "F7 shows the shot debugger")
	hold(["move_forward"]); me.airborne = true; await get_tree().process_frame; await get_tree().process_frame
	var dt: String = sh.get_debug_text()
	check(dt.contains("AIRBORNE") and dt.contains("FORWARD (topspin)") and dt.contains("your touch now -> topspin") and dt.contains("move_forward["), "debugger shows held keys, airborne, intent: " + dt.get_slice("\n", 3))
	me.airborne = false; hold([])
	check(sh.get_debug_events().size() >= 1 and str(sh.get_debug_events()[0]).contains("jump_serve"), "debugger lists the last touches (%s)" % str(sh.get_debug_events()[0] if sh.get_debug_events().size() else ""))
	var lg := FileAccess.get_file_as_string("user://jv_mods/jv_mods.log")
	check(lg.contains("[jv-mods:shotdbg] custom shot debugger ON") and lg.contains("[jv-mods:shotdbg] held"), "debugger state goes to jv_mods.log")
	var f7u := InputEventKey.new(); f7u.keycode = KEY_F7; f7u.pressed = true; Input.parse_input_event(f7u)
	await get_tree().process_frame
	check(not sh.is_debug_visible(), "F7 again hides it")
	_freeplay = false; await wait(0.5)
	await wait(0.7)

	var cam := Camera3D.new(); add_child(cam); cam.current = true
	cam.global_transform = Transform3D(Basis(), Vector3(0, 3, 8))
	gball.curve = 3.0
	hold(["sprint", "strafe", "move_right"]); await touch(me, 1); hold([])
	check(sh.get_last_event() == "applied low_shot" and absf(gball.curve) < 12.0 - 0.01, "curve: GROUND touch with right held -> no camera curve (game's own curve kept, %.2f)" % gball.curve)
	me.airborne = true; hold(["sprint", "strafe", "move_right"]); await touch(me, 1); hold([]); me.airborne = false
	var c1: float = sh.get_last_curve()
	check(c1 < -1.0, "curve: airborne, camera looking -Z, hold right -> bends to camera-right (curve %.2f < 0)" % c1)
	me.airborne = true; hold(["sprint", "strafe", "move_left"]); await touch(me, 1); hold([]); me.airborne = false
	check(sh.get_last_curve() > 1.0, "curve: airborne, hold left -> bends left (%.2f)" % sh.get_last_curve())
	cam.global_transform = Transform3D(Basis(Vector3.UP, PI), Vector3(0, 3, -14))
	me.airborne = true; hold(["sprint", "strafe", "move_right"]); await touch(me, 1); hold([]); me.airborne = false
	check(sh.get_last_curve() > 1.0, "curve: camera turned around, hold right -> camera-right = flight's left (%.2f > 0), not world axes" % sh.get_last_curve())


	me.airborne = true; hold(["move_back", "move_left"]); await touch(me, 1); hold([]); me.airborne = false
	check(sh.get_last_event() == "applied topspin" and sh.get_last_curve() < -1.0, "curve: turned camera, S+A walk you toward the net and right -> topspin bending right (%.2f)" % sh.get_last_curve())

	var cr: Vector3 = cam.global_basis.x; cr.y = 0; cr = cr.normalized()
	me.global_position = Vector3(0, 1.5, 4); gball.global_position = Vector3(0, 2.5, 4)
	me.aim_point = Vector3(0, 0, -5)
	var ss0 := int(sh.get_applied_count())
	hold(["move_right"]); await touch(me, 3, 0.5); hold([])
	check(int(sh.get_applied_count()) == ss0, "curveball: right held on a GROUND touch = the game's shot (%s)" % sh.get_last_event())
	var lat := []
	for hp in [0.05, 0.5, 1.0]:
		me.fake_hold = hp
		me.airborne = true; hold(["move_right"]); await touch(me, 3, hp); hold([]); me.airborne = false
		var info: Dictionary = sh.get_last_spin()
		lat.append((info["target"] - me.aim_point).dot(cr))
	check(sh.get_last_event() == "applied curveball", "curveball: airborne + right held = Curveball (%s)" % sh.get_last_event())
	check(lat[0] >= 0.0 and lat[0] < 0.6 and lat[1] > lat[0] and lat[2] > lat[1], "curveball: tap barely moves, longer hold goes further toward camera-right (%s)" % str(lat))
	var info2: Dictionary = sh.get_last_spin()
	var landed: Vector3 = gball._predict_land_curved()
	check(landed.distance_to(info2["landing"]) < 0.3, "curveball: the game's own curved prediction lands on the sampled point (%s vs %s)" % [landed, info2["landing"]])
	check(signf(gball.curve) == signf(-1.0 * (gball.vel.normalized().cross(Vector3.UP)).dot(cr)) or absf(gball.curve) > 0.1, "curveball: curve set toward the spin side (%.2f)" % gball.curve)
	me.fake_hold = 0.6
	me.airborne = true; hold(["move_left"]); await touch(me, 3, 0.6); hold([]); me.airborne = false
	check(((sh.get_last_spin()["target"] - me.aim_point) as Vector3).dot(cr) < -1.0, "curveball: left held goes toward camera-left")
	me.fake_quality = 1.0; me.airborne = true; hold(["move_right"]); await touch(me, 3, 0.5); hold([]); me.airborne = false
	var good: float = sh.get_last_spin()["scale"]
	me.fake_quality = 0.2; me.airborne = true; hold(["move_right"]); await touch(me, 3, 0.5); hold([]); me.airborne = false
	var bad: float = sh.get_last_spin()["scale"]
	check(bad > good, "curveball: sloppy touch widens the spread (%.2f > %.2f)" % [bad, good])
	me.fake_quality = 0.8; me.fake_hold = 0.5



	cam.global_transform = Transform3D(Basis(), Vector3(0, 3, 8))
	var lp: MeshInstance3D = gball._land_predict
	var bpos := Vector3(0, 2.5, 4)
	me.global_position = Vector3(0, 1.5, 4); gball.global_position = bpos; gball.vel = Vector3.ZERO; gball.curve = 0.0
	me.airborne = true; hold(["move_forward"]); await touch(me, 3, 0.7); hold([]); me.airborne = false
	gball.frozen = false; await get_tree().physics_frame; await get_tree().physics_frame
	var exact: Vector3 = gball._predict_land_curved()
	var sd0: Dictionary = sh.get_spin_display()
	check(sh.get_last_event() == "applied topspin" and not bool(sh.get_range_box()["on"]) and not bool(sd0.get("on", false)) and lp.visible and Vector2(lp.global_position.x - exact.x, lp.global_position.z - exact.z).length() < 0.05 and is_equal_approx(gball.box_spread(), 1.15),
		"marker: topspin keeps the game's EXACT landing marker (marker %s, predicted %s, spread %.2f, spin %s)" % [lp.global_position, exact, gball.box_spread(), sd0])
	check(st.get("spin_display") == "question" and st.get("curve_display_remote") == "grey", "shots: default spin_display=question, curve_display_remote=grey (%s)" % st)
	check(sh.validate_config({"curve_display_remote": "purple"}).find("grey") != -1, "shots: curve_display_remote rejects unknown values")
	check(sh.validate_config({"spin_display": "maybe"}).find("question") != -1, "shots: spin_display rejects unknown values")
	check(sh.validate_config({"curve_display_remote": "grey", "spin_display": "question"}) == "", "shots: default visual options validate")
	gball.frozen = true; gball.global_position = bpos; gball.vel = Vector3.ZERO; gball.curve = 0.0; await get_tree().physics_frame
	me.fake_hold = 0.8; me.fake_quality = 0.8
	me.airborne = true; hold(["move_right"]); await touch(me, 3, 0.8); hold([]); me.airborne = false
	var sp: Dictionary = sh.get_last_spin()
	gball.frozen = false; await get_tree().physics_frame; await get_tree().physics_frame; await get_tree().physics_frame
	var land: Vector3 = gball._predict_land_curved()
	var sd: Dictionary = sh.get_spin_display()
	var qpos: Vector3 = sd.get("question_pos", Vector3.ZERO)
	check(sh.get_last_event() == "applied curveball" and bool(sd.get("on")) and str(sd.get("mode")) == "question" and bool(sd.get("local")) and str(sd.get("style")) == "full" and bool(sd.get("question")) and bool(sd.get("path")),
		"marker: Curveball uses the question spin display for the hitter (full colour) (%s)" % str(sd))
	check(Vector2(qpos.x - land.x, qpos.z - land.z).length() >= 0.25 and not lp.visible,
		"marker: the ? sits off the exact landing and the exact ring is hidden (? %s, landing %s, hidden %s)" % [qpos, land, not lp.visible])


	mate.aim_point = me.aim_point
	gball.hit_by = mate; gball.strike_source = mate
	await get_tree().physics_frame; await get_tree().physics_frame; await get_tree().physics_frame
	var rsd: Dictionary = sh.get_spin_display()
	var rland: Vector3 = gball._predict_land_curved()
	var rq: Vector3 = rsd.get("question_pos", Vector3.ZERO)
	var b0: Vector3 = rsd.get("box_start", Vector3.ZERO)
	var b1: Vector3 = rsd.get("box_end", Vector3.ZERO)
	var aim0: Vector3 = me.aim_point
	var far0: Vector3 = sp.get("path_end", Vector3.ZERO)
	check(bool(rsd.get("on")) and rsd.get("local") == false and str(rsd.get("style")) == "grey" and bool(rsd.get("question")) and not bool(rsd.get("path")) and not bool(rsd.get("path_visible", true)) and bool(rsd.get("box")),
		"marker: a remote curve shot has no path, only the uncertainty box + ? (%s, land %s)" % [str(rsd), rland])
	check(Vector2(rq.x - rland.x, rq.z - rland.z).length() < 0.5,
		"marker: remote ? stays near the true landing (? %s, land %s)" % [rq, rland])
	check(Vector2(b0.x - aim0.x, b0.z - aim0.z).length() < 0.35 and Vector2(b1.x - far0.x, b1.z - far0.z).length() < 0.6,
		"marker: remote box spans the crosshair to the max landing (box %s -> %s, aim %s, far %s)" % [b0, b1, aim0, far0])
	check(lp.visible, "marker: remote uncertainty box is the game's own landing-marker nodes (visible)")

	gball.frozen = true; gball.global_position = bpos; gball.vel = Vector3.ZERO; gball.curve = 0.0; gball.hit_by = me; gball.strike_source = me
	me.airborne = true; hold(["move_right"]); await touch(me, 3, 0.8); hold([]); me.airborne = false
	gball.frozen = false; await get_tree().physics_frame; await get_tree().physics_frame; await get_tree().physics_frame
	var f_early: float = float(sh.get_spin_display().get("fade", -1))
	for i in 30: await get_tree().physics_frame
	var f_late: float = float(sh.get_spin_display().get("fade", -1))
	check(f_early > 0.5 and f_late < 0.2, "marker: the spin display fades out like the float-serve box (fade %.2f -> %.2f)" % [f_early, f_late])


	gball.curve = 0.0; gball.frozen = true; gball.global_position = Vector3(0, 0.13, 4); gball.vel = Vector3.ZERO
	await get_tree().physics_frame; await get_tree().physics_frame
	await touch(mate, 3, 0.5); gball.curve = 0.0; gball.frozen = true
	await get_tree().physics_frame; await get_tree().physics_frame
	check(not bool(sh.get_range_box()["on"]) and not bool(sh.get_spin_display().get("on", false)),
		"marker: the next touch (a teammate's, no curve) gives the game back its own marker (box %s spin %s)" % [str(sh.get_range_box()), str(sh.get_spin_display())])

	gball.frozen = true; gball.global_position = bpos; gball.vel = Vector3.ZERO; gball.curve = 0.0; await get_tree().physics_frame
	me.airborne = true; hold(["move_forward", "move_right"]); await touch(me, 3, 0.7); hold([]); me.airborne = false
	gball.frozen = false; await get_tree().physics_frame; await get_tree().physics_frame; await get_tree().physics_frame
	var tsd: Dictionary = sh.get_spin_display()
	var tland: Vector3 = gball._predict_land_curved()
	var tq: Vector3 = tsd.get("question_pos", Vector3.ZERO)
	check(sh.get_last_event() == "applied topspin" and absf(sh.get_last_curve()) > 0.05 and bool(tsd.get("on")) and bool(tsd.get("question")) and Vector2(tq.x - tland.x, tq.z - tland.z).length() >= 0.25 and not lp.visible,
		"marker: topspin-with-curve uses the offset ? display, not the exact ring (curve %.2f, %s, land %s)" % [sh.get_last_curve(), str(tsd), tland])
	gball.frozen = true; gball.curve = 0.0; gball.global_position = bpos; gball.vel = Vector3.ZERO; await get_tree().physics_frame

	var shots_dir := OS.get_environment("JV_MODS_DIR").path_join("shots")
	var defp := shots_dir.path_join("00-default.json")
	var def0: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(defp))
	def0["spin_display"] = "exact"
	var wf := FileAccess.open(defp, FileAccess.WRITE); wf.store_string(JSON.stringify(def0)); wf.close()
	check(sh.reload_config(), "shots: reload_config applies spin_display=exact")
	me.airborne = true; hold(["move_right"]); await touch(me, 3, 0.8); hold([]); me.airborne = false
	gball.frozen = false; await get_tree().physics_frame; await get_tree().physics_frame; await get_tree().physics_frame
	var esd: Dictionary = sh.get_spin_display()
	var erb: Dictionary = sh.get_range_box()
	check(str(esd.get("mode")) == "exact" and not bool(esd.get("question")) and bool(erb.get("on")),
		"marker: spin_display=exact keeps the Curveball range box / bar, no ? (%s / %s)" % [str(esd), str(erb)])
	def0["spin_display"] = "question"; def0["curve_display_remote"] = "off"
	wf = FileAccess.open(defp, FileAccess.WRITE); wf.store_string(JSON.stringify(def0)); wf.close()
	check(sh.reload_config(), "shots: reload_config applies curve_display_remote=off")
	gball.hit_by = mate; gball.strike_source = mate; gball.curve = 8.0; gball.frozen = false
	await get_tree().physics_frame; await get_tree().physics_frame
	var osd: Dictionary = sh.get_spin_display()
	check(str(osd.get("style")) == "off" and not bool(osd.get("question")) and not bool(osd.get("path_visible", true)) and not lp.visible,
		"marker: curve_display_remote=off hides the remote spin display and the exact marker (%s, lp.visible=%s)" % [str(osd), lp.visible])
	def0["spin_display"] = "question"; def0["curve_display_remote"] = "grey"
	wf = FileAccess.open(defp, FileAccess.WRITE); wf.store_string(JSON.stringify(def0)); wf.close()
	sh.reload_config()
	gball.curve = 0.0; gball.frozen = true; gball.hit_by = me; gball.strike_source = me
	gball.frozen = true; gball.global_position = bpos; gball.vel = Vector3.ZERO; await get_tree().physics_frame

	var sb0 := int(sh.get_applied_count())
	hold(["move_back"]); await touch(me, 3, 0.5); hold([])
	check(int(sh.get_applied_count()) == sb0 and not gball.float_serve, "float: back on a GROUND touch = the game's own shot (%s)" % sh.get_last_event())
	me.airborne = true; hold(["move_back"]); await touch(me, 2, 0.5); hold([]); me.airborne = false
	check(int(sh.get_applied_count()) == sb0 and not gball.float_serve, "float: an airborne SET with back held stays the game's back set (%s)" % sh.get_last_event())
	me.airborne = true; hold(["move_back"]); await touch(me, 3, 0.5); hold([]); me.airborne = false
	var lf: Dictionary = sh.get_last_float()
	check(sh.get_last_event() == "applied float" and gball.float_serve and gball.float_seed == int(lf.get("seed", -1)) and absf(float(lf.get("charge", -1)) - 0.5833) < 0.01,
		"float: airborne + back = the game's own float (start_float seed %d, charge %.2f = inverse_lerp(0.15, 0.75, 0.5))" % [gball.float_seed, float(lf.get("charge", -1))])
	check((gball.vel - Ball.solve_float(gball.global_position, Vector3(me.aim_point.x, 0, me.aim_point.z), 0.5)).length() < 0.01 and gball.curve == 0.0, "float: launched by the game's Ball.solve_float toward the aim point, no curve (%s)" % gball.vel)
	gball.frozen = false; await get_tree().physics_frame; await get_tree().physics_frame
	check(not bool(sh.get_range_box()["on"]) and gball.box_spread() > 2.0, "float: the game's own float-serve box shows (spread %.2f)" % gball.box_spread())
	me.airborne = true; hold(["move_back", "move_left"]); await touch(me, 3, 0.5); hold([]); me.airborne = false
	check(sh.get_last_event() == "applied float", "float: back + left (equal) = Float, not Curveball: one air intent per touch (%s)" % sh.get_last_event())
	me.airborne = true; hold(["move_forward", "move_back"]); var nb0 := int(sh.get_applied_count()); await touch(me, 3, 0.5); hold([]); me.airborne = false
	check(int(sh.get_applied_count()) == nb0, "float: forward + back cancel (no intent) = the game's shot")
	gball.frozen = true; gball.global_position = bpos; gball.vel = Vector3.ZERO; gball.stop_float(); await get_tree().physics_frame
	cam.queue_free()

	me.sim_authority = false
	var a1 := int(sh.get_applied_count())
	hold(["sprint", "strafe"]); await touch(me); hold([])
	check(int(sh.get_applied_count()) == a1 and sh.get_last_event().begins_with("skipped"), "shots: skipped where the player has no sim_authority (%s)" % sh.get_last_event())
	me.sim_authority = true

	hold(["sprint", "strafe"]); await touch(mate); hold([])
	check(int(sh.get_applied_count()) == a1, "shots: a teammate's touch does not use your shot")

	var vp := SubViewport.new(); add_child(vp)
	var emb: Node3D = load("res://fake_main.gd").new(); vp.add_child(emb)
	var eball := Ball.new(); emb.add_child(eball); emb.ball = eball
	var ep := Player.new(); ep.ball = eball; emb.add_child(ep); emb.player = ep
	emb._rally = Rally.new(); emb._rally.players = [ep]
	ep.touched.connect(func(k): emb._rally.judge_touch(k, ep))
	await wait(1.2)
	check(int(sh.get_status().get("balls_hooked", 0)) == 2, "shots: practice-court ball hooked too (got %s)" % sh.get_status().get("balls_hooked"))
	var a2 := int(sh.get_applied_count())
	hold(["sprint", "strafe"]); ep.do_touch(1); await get_tree().physics_frame; hold([])
	check(int(sh.get_applied_count()) == a2 + 1, "shots: applied for the practice court's own player (owner scene's `player`)")
	vp.queue_free(); await frame()

	_rally = Rally.new(); _rally.players = [me, mate, foe]
	var g := goofy()

	check(is_equal_approx(ph.radius, r0) and is_equal_approx(ph.gravity, 11.0) and is_equal_approx(ph.floor_bounce, 0.5), "goofy defaults: BallPhysics untouched (radius %.3f gravity %.2f bounce %.2f)" % [ph.radius, ph.gravity, ph.floor_bounce])
	check(gball.scale.is_equal_approx(Vector3.ONE) and is_equal_approx(gball.visual_radius(), r0), "goofy defaults: ball node scale 1, visual radius %.3f = vanilla" % gball.visual_radius())
	check(int(g.get_status().get("features", {}).get("ball_physics", -1)) == 0, "goofy defaults: no BallPhysics property edited (%s)" % str(g.get_status().get("features")))

	var gf := FileAccess.open(OS.get_environment("JV_MODS_DIR").path_join("goofy").path_join("50-test.json"), FileAccess.WRITE)
	gf.store_string('{"big_ball": 1.5, "ball_physics": {"gravity_scale": 0.85, "bounce_scale": 1.3}}'); gf.close()
	check(g.reload_config(), "goofy: reload_config applies the user's file now")
	await wait(0.6)
	check(is_equal_approx(ph.radius, r0 * 1.5), "goofy big_ball: BallPhysics.radius %.3f (x1.5)" % ph.radius)
	check(gball.scale.is_equal_approx(Vector3.ONE * 1.5), "goofy big_ball: ball dressed before the edit gets node scale %s" % gball.scale)
	check(absf(gball.visual_radius() - r0 * 1.5) < 0.002, "goofy big_ball: old ball's visual radius %.3f = hitbox %.3f" % [gball.visual_radius(), ph.radius])
	var nb := Ball.new(); nb.name = "LateBall"; add_child(nb)
	await wait(0.6)
	check(nb.scale.is_equal_approx(Vector3.ONE) and absf(nb.visual_radius() - r0 * 1.5) < 0.002, "goofy big_ball: ball dressed after the edit is NOT double-scaled (node %s, visual %.3f, was x2.25 = %.3f)" % [nb.scale, nb.visual_radius(), r0 * 2.25])
	nb.queue_free()
	check(is_equal_approx(ph.gravity, 11.0 * 0.85) and is_equal_approx(ph.floor_bounce, 0.5 * 1.3), "goofy ball_physics: gravity %.2f bounce %.2f" % [ph.gravity, ph.floor_bounce])
	check(is_equal_approx(Engine.time_scale, 1.0), "goofy time_scale stays opt-in (1.0)")
	check(me.scale.is_equal_approx(Vector3.ONE), "goofy player_scale default 1.0 (opt-in)")
	gball.position = Vector3(0, 3, 0); gball.vel = Vector3.ZERO; gball.dive_g = 1.0; gball.frozen = false
	await wait(0.5); gball.frozen = true
	check(gball.vel.y < -3.0 and gball.vel.y > -11.0 * 0.5, "goofy: scripted ball falls with the edited BallPhysics gravity (vy %.2f)" % gball.vel.y)

	await new_rally()
	await touch(me); await touch(mate); await touch(me)
	check(_rally.touch_count == 3 and _rally.fault == "", "rules off: game counts 3 touches")
	await touch(mate)
	check(_rally.fault == "four touches", "rules off: 4th touch is the game's 'four touches' fault")
	await new_rally()
	await touch(me); var dbl := await touch(me)
	check(not dbl, "rules off: game bars a consecutive touch (CONSECUTIVE)")

	var panel := ModifiersPanel.new(); panel.name = "ModifiersPanel"; add_child(panel)
	await wait(0.4)
	check(panel._rows.has("jvm_max_hits") and panel._rows.has("jvm_no_double"), "panel: max hits + no double-hit rows injected via _entry_row")
	var sel: UiSelect = panel._rows.get("jvm_max_hits")
	check(sel != null and sel.options.size() == 11 and sel.options[0] == "off" and sel.selected == 0, "panel: max hits options off,1..10, shows off")
	check(panel.list.get_child_count() == 7, "panel: 2 rows + 2 separators appended (%d children)" % panel.list.get_child_count())
	await wait(0.4)
	check(panel.list.get_child_count() == 7, "panel: no duplicate rows on later ticks")
	sel.picked.emit(2); await frame()
	check(g.get_max_hits() == 2, "panel: picking 2 sets max hits per team")
	await new_rally()
	await touch(me); await touch(mate)
	check(_rally.fault == "" and _rally.touch_count == 3, "max hits 2: two touches used up the side (count %d)" % _rally.touch_count)
	await touch(me)
	check(_rally.fault == "four touches", "max hits 2: 3rd touch is a fault")
	sel.picked.emit(5); await frame()
	await new_rally()
	for p in [me, mate, me, mate, me]: await touch(p)
	check(_rally.fault == "", "max hits 5: five touches allowed (count %d)" % _rally.touch_count)
	await touch(mate)
	check(_rally.fault == "four touches", "max hits 5: 6th touch is a fault")
	await new_rally(); await touch(me); await touch(foe)
	check(_rally.touch_side == -1 and _rally.touch_count == 1 and _rally.fault == "", "max hits: side change resets the count")
	var cb: CheckBox = panel._rows.get("jvm_no_double")
	cb.button_pressed = true; await frame()
	check(g.get_no_double_hit(), "panel: no double-hit toggle on")
	await new_rally()
	var t1 := await touch(me); var t2 := await touch(me)
	check(t1 and t2 and _rally.fault == "", "no double-hit: same player touches twice in a row")
	for i in 3: await touch(me)
	check(_rally.fault == "", "no double-hit: 5 touches by one player allowed with max hits 5")
	await touch(me)
	check(_rally.fault == "four touches", "no double-hit: consecutive touches still count toward max hits (fault on the 6th)")

	Net.active = true; Net.is_host = false; Net.party_lan = true
	await wait(0.9)
	await new_rally(); await touch(me); var c2 := await touch(me)
	check(not c2, "client: rules not enforced locally (host decides)")
	Net.is_host = true
	await wait(0.6)
	await new_rally(); await touch(me); c2 = await touch(me)
	check(c2, "LAN host: rules enforced")

	reset_menu(); Net.active = true; Net.custom_lobby = true; Net.privacy = 0; Net.is_host = true
	await wait(1.0)
	var row: Control = panel._rows["jvm_max_hits"].get_parent()
	check(not row.visible, "public lobby: modifier rows hidden")
	check(is_equal_approx(ph.radius, r0) and is_equal_approx(ph.gravity, 11.0) and gball.scale.is_equal_approx(Vector3.ONE), "public lobby: BallPhysics + ball scale restored")
	await new_rally(); await touch(me); c2 = await touch(me)
	check(not c2, "public lobby: no double-hit not enforced")
	var ap := int(sh.get_applied_count())
	hold(["sprint", "strafe"]); await touch(foe); await touch(me); hold([])
	check(int(sh.get_applied_count()) == ap, "public lobby: combos do nothing")
	check(InputMap.action_get_events("move_forward").size() == 2, "public lobby: shots off, forward movement normal")
	reset_menu(); Net.is_host = false
	await wait(1.0)
	check(row.visible and is_equal_approx(ph.radius, r0 * 1.5), "back to local: rows visible, big ball back")
	check(InputMap.action_get_events("move_forward").size() == 2, "back to local: forward still walks")
	await _goofy_ball(g, ph, r0)
	var gs: Dictionary = g.get_status()
	print("  goofy status: ", gs)
	check(int(gs.get("features", {}).get("rule_writes", 0)) > 0, "goofy status reports rule writes")


var _gb_changed := 0
var _gev: Array = []


func _goofy_libs(g: Node, ph: Resource, _r0: float) -> void:
	var feel = load("res://game/player/player_feel.tres")
	var me: Player = _rally.players[0]
	check(g.get_ball_schema().size() == 156 and g.get_player_schema().size() == 195 and g.get_schema().size() == 351, "goofy 1.2: ball 156 + player 195 params (%d / %d)" % [g.get_ball_schema().size(), g.get_player_schema().size()])
	var ps: Array = g.get_player_schema()
	check(ps.any(func(p): return p["id"] == "player.move_speed" and p["library"] == "player" and p["type"] == "float" and p["section"] == "movement"), "player schema: player.move_speed (movement, float)")
	check(ps.any(func(p): return p["id"] == "player.contact_queue_steps" and p["type"] == "int") and ps.any(func(p): return p["id"] == "player.body_scale" and p["section"] == "body"), "player schema: integer + body scale params")
	check(g.validate_config({"player_params": {"player.nope": 1}}).contains("unknown") and g.validate_config({"player_params": {"player.move_speed": -3}}).contains("between"), "goofy editor: bad player_params refused")
	var path := OS.get_environment("JV_MODS_DIR").path_join("goofy").path_join("61-player.json")
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string('{"player_scale": 1.25, "player_params": {"player.move_speed": 8.0, "player.contact_queue_steps": 9.4, "player.indicator_color": [1, 0, 1, 1], "player.jump_min_velocity": {"mul": 2}}}'); f.close()
	g.reload_config(); await wait(0.7)
	check(is_equal_approx(feel.move_speed, 8.0) and is_equal_approx(feel.jump_min_velocity, 15.0), "player_params: move_speed %.2f, jump_min_velocity %.2f (x2)" % [feel.move_speed, feel.jump_min_velocity])
	check(typeof(feel.contact_queue_steps) == TYPE_INT and feel.contact_queue_steps == 9, "player_params: integer param rounded (%s)" % str(feel.contact_queue_steps))
	check(feel.indicator_color.is_equal_approx(Color(1, 0, 1, 1)), "player_params: colour param (%s)" % feel.indicator_color)
	check(me.scale.is_equal_approx(Vector3.ONE * 1.25) and is_equal_approx(float(g.get_param("player.body_scale")), 1.25), "player_scale -> player.body_scale 1.25 (node %s)" % me.scale)
	var r: Dictionary = g.set_player_override("mod:pm:player", {"player.move_speed": 3.0, "ball.gravity": 1.0, "player.body_scale": 2.0})
	await wait(0.6)
	check(is_equal_approx(feel.move_speed, 3.0) and str(r.get("errors")).contains("ball.gravity") and not is_equal_approx(ph.gravity, 1.0), "set_player_override: player params only, ball id refused (%s)" % str(r.get("errors")))
	check(me.scale.is_equal_approx(Vector3.ONE * 2.0), "body scale follows live (%s)" % me.scale)
	check(g.clear_override("mod:pm:player"), "player layer cleared")
	await wait(0.6)
	check(is_equal_approx(feel.move_speed, 8.0) and me.scale.is_equal_approx(Vector3.ONE * 1.25), "back to the user's player params (%.2f, %s)" % [feel.move_speed, me.scale])
	g.set_loader_map("1090"); g.set_ball_mode("fixed"); await wait(0.2)
	check(is_equal_approx(ph.gravity, 11.0) and is_equal_approx(feel.move_speed, 8.0), "FIXED is a ball override: player library kept (g %.2f move %.2f)" % [ph.gravity, feel.move_speed])
	g.set_ball_mode("normal"); g.set_loader_map(""); await wait(0.2)

	var pl: Array = g.list_players()
	var mi := -1
	for i in pl.size(): if (pl[i]["position"] as Vector3).is_equal_approx(me.global_position) and pl[i].get("team_side") == me.team_side: mi = i
	check(mi >= 0 and pl[mi].has("velocity") and pl[mi].has("grounded") and pl[mi]["anim"].get("clip") == "idle" and is_equal_approx(float(pl[mi]["body_scale"]), 1.25), "player state: position, velocity, grounded, anim %s" % str(pl[mi] if mi >= 0 else {}))
	var cos: Dictionary = g.get_player_cosmetics(mi)
	check(cos.get("display_name") == "Fakey" and cos.get("accessories", {}).get("head") == "cap_red" and cos.get("look", {}).has("skin"), "player cosmetics: name, accessories, look (%s)" % str(cos).left(160))
	gball.position = Vector3(1.234, 5.0, 0.5); gball.frozen = true
	var bl: Array = g.list_balls()
	var bi := -1
	for i in bl.size(): if (bl[i]["position"] as Vector3).is_equal_approx(gball.global_position): bi = i
	check(bi >= 0 and bl[bi]["frozen"] == true and bl[bi]["airborne"] == true and is_equal_approx(float(bl[bi]["radius"]), ph.radius), "ball state: frozen, airborne, radius (%s)" % str(bl[bi] if bi >= 0 else {}).left(200))
	check(g.set_ball_velocity(bi, Vector3(1, 2, 3)) and gball.vel.is_equal_approx(Vector3(1, 2, 3)), "set_ball_velocity (offline)")
	check(g.apply_ball_impulse(bi, Vector3(0, 1, 0)) and gball.vel.is_equal_approx(Vector3(1, 3, 3)), "apply_ball_impulse")
	check(g.set_ball_curve(bi, 2.5) and is_equal_approx(gball.curve, 2.5) and is_equal_approx(float(g.get_ball_state(bi)["curve"]), 2.5), "set_ball_curve + state read back")
	check(not g.set_ball_velocity(bi, Vector3(NAN, 0, 0)) and not g.set_ball_velocity(999, Vector3.ONE), "live writes: NaN / bad index refused")

	g.game_event.connect(func(k, n, i, a): _gev.append([k, n, i, a]))
	await wait(0.7)
	gball.bounced.emit(); gball.antenna_hit.emit(-1); me.jumped.emit(true)
	await frame()
	check(_gev.has(["ball", "bounced", bi, []]) and _gev.has(["ball", "antenna_hit", bi, [-1]]) and _gev.has(["player", "jumped", mi, [true]]), "game events relayed with index + args (%s)" % str(_gev).left(200))
	DirAccess.remove_absolute(path); g.reload_config(); await wait(0.7)
	check(is_equal_approx(feel.move_speed, 5.9) and feel.contact_queue_steps == 6 and me.scale.is_equal_approx(Vector3.ONE), "player file removed: player library stock, body scale 1")



func _goofy_host_sync(g: Node, ph: Resource, r0: float) -> void:
	var feel = load("res://game/player/player_feel.tres")
	var red := Color(1, 0, 0, 1)
	Net.active = true; Net.party_lan = true; Net.is_host = true; Net.in_match = false
	await wait(0.9)
	var st: Dictionary = g.get_sync_status()
	check(gate().is_active() and g.is_physics_allowed() and st.get("role") == "host" and is_equal_approx(ph.gravity, 11.0 * 0.85) and is_equal_approx(feel.reach, 2.3), "LAN host: libraries live, own values published (role %s, g %.2f)" % [st.get("role"), ph.gravity])
	var schema: Array = g.get_schema()
	var vis: Array = schema.filter(func(p): return p["sync"] == "local").map(func(p): return p["id"])
	check(vis.has("ball.outline_home") and vis.has("ball.trail_min_speed") and vis.has("player.indicator_color") and not vis.has("ball.gravity") and not vis.has("flight.air_drag") and schema.filter(func(p): return not p["sync"] in ["host", "local"]).is_empty(), "schema: every param classified host / local (%d visual-only)" % vis.size())
	Net.is_host = false
	await wait(0.9)
	st = g.get_sync_status()
	check(st.get("role") == "client" and st.get("authoritative") == "local" and not st.get("live") and is_equal_approx(ph.gravity, 11.0 * 0.85), "LAN client in the lobby (no match): own values (g %.2f, %s)" % [ph.gravity, st.get("authoritative")])
	var ts0 := Engine.time_scale
	Net.in_match = true
	await wait(0.5)
	st = g.get_sync_status()
	check(st.get("live") and st.get("authoritative") == "host" and is_equal_approx(ph.gravity, 11.0) and is_equal_approx(feel.reach, 1.15) and is_equal_approx(ph.radius, r0) and ph.outline_home.is_equal_approx(red), "LAN client, match started, no host values yet: gameplay stock, own visual kept (g %.2f r %.3f outline %s)" % [ph.gravity, ph.radius, ph.outline_home])
	var pk := func(d) -> PackedByteArray: return "JVGF".to_utf8_buffer() + JSON.stringify(d).to_utf8_buffer()
	var mx := float(g.get_ball_schema().filter(func(p): return p["id"] == "flight.max_speed")[0]["max"])
	var rej0 := int(g.get_sync_status()["rejected"])
	g._on_sync_packet(1, pk.call({"v": 2, "sid": 7, "seq": 1, "ts": 1.75, "params": {"ball.gravity": 30.0, "player.move_speed": 7.5, "flight.air_drag": 0.4, "ball.floor_bounce": 0.9, "ball.radius": 0.4, "ball.outline_home": [0, 1, 0], "flight.max_speed": 1e12}}))
	await frame()
	check(is_equal_approx(ph.gravity, 30.0) and is_equal_approx(feel.move_speed, 7.5) and is_equal_approx(float(g.get_param("flight.air_drag")), 0.4) and is_equal_approx(ph.floor_bounce, 0.9) and is_equal_approx(ph.radius, 0.4) and is_equal_approx(float(g.get_param("flight.max_speed")), mx), "LAN client in match: host gravity / drag / bounce / hitbox / movement applied, range clamped (g %.2f max_speed %s)" % [ph.gravity, g.get_param("flight.max_speed")])
	check(ph.outline_home.is_equal_approx(red) and int(g.get_sync_status()["rejected"]) == rej0 + 1, "visual-only param from the host dropped, own look kept (%s)" % ph.outline_home)
	check(is_equal_approx(Engine.time_scale, 1.75) and g.get_sync_status()["time_scale_synced"], "host time scale applied on the client (%.2f)" % Engine.time_scale)
	Engine.time_scale = 1.0
	await frame(); await frame()
	check(is_equal_approx(Engine.time_scale, 1.75), "time scale re-asserted after the game's slow-mo reset (%.2f)" % Engine.time_scale)
	var rej := int(g.get_sync_status()["rejected"])
	g._on_sync_packet(5, pk.call({"v": 2, "sid": 7, "seq": 2, "ts": 1.0, "params": {"ball.gravity": 1.0}}))
	g._on_sync_packet(1, pk.call({"v": 1, "sid": 7, "seq": 3, "ts": 1.0, "params": {"ball.gravity": 2.0}}))
	g._on_sync_packet(1, pk.call({"v": 2, "sid": 7, "seq": 1, "ts": 1.0, "params": {"ball.gravity": 3.0}}))
	g._on_sync_packet(1, pk.call({"v": 2, "sid": 7, "seq": 9, "ts": NAN, "params": {"ball.gravity": 4.0}}))
	var big := "JVGF".to_utf8_buffer(); big.resize(40000)
	g._on_sync_packet(1, big)
	g._on_sync_packet(1, "JVGF{not json".to_utf8_buffer())
	await frame()
	check(is_equal_approx(ph.gravity, 30.0) and is_equal_approx(Engine.time_scale, 1.75) and int(g.get_sync_status()["rejected"]) >= rej + 5, "rejected: not from host, old version, bad time scale, oversize, bad JSON; replay ignored (g %.2f, rejected %d)" % [ph.gravity, int(g.get_sync_status()["rejected"]) - rej])
	g._on_sync_packet(1, pk.call({"v": 2, "sid": 7, "seq": 10, "ts": 99, "params": {"ball.gravity": "OS.execute('rm', ['-rf', '/'])", "ball.nope": 1, "ball.floor_bounce": [1, 2], "player.move_speed": NAN, "ball.radius": {"mul": 9}}}))
	await frame()
	check(is_equal_approx(ph.gravity, 11.0) and is_equal_approx(feel.move_speed, 5.9) and is_equal_approx(ph.radius, r0) and int(g.get_status()["features"]["sync"]["host_values"]) == 0 and is_equal_approx(Engine.time_scale, 4.0), "invalid values dropped, nothing evaluated: stock; time scale clamped to 4 (g %.2f ts %.2f)" % [ph.gravity, Engine.time_scale])
	g._on_sync_packet(1, pk.call({"v": 2, "sid": 7, "seq": 11, "ts": 1.75, "params": {"player.body_scale": 1.5, "ball.gravity": 30.0}}))
	await wait(0.6)
	check((_rally.players[0] as Player).scale.is_equal_approx(Vector3.ONE * 1.5), "host's body scale applied on the client")

	Net.in_match = false
	await wait(0.5)
	st = g.get_sync_status()
	check(not st.get("live") and is_equal_approx(ph.gravity, 11.0 * 0.85) and is_equal_approx(feel.reach, 2.3) and is_equal_approx(Engine.time_scale, ts0) and (_rally.players[0] as Player).scale.is_equal_approx(Vector3.ONE), "match ended: own values and time scale restored (g %.2f ts %.2f)" % [ph.gravity, Engine.time_scale])
	Net.practice_live = true
	await wait(0.5)
	check(is_equal_approx(ph.gravity, 30.0) and is_equal_approx(Engine.time_scale, 1.75), "lobby practice: host values again (g %.2f ts %.2f)" % [ph.gravity, Engine.time_scale])
	g._on_sync_peer_gone(1)
	await frame()
	check(is_equal_approx(ph.gravity, 11.0 * 0.85) and is_equal_approx(Engine.time_scale, ts0) and g.get_sync_status()["host_left"], "host left mid-session: own values and time scale restored (g %.2f ts %.2f)" % [ph.gravity, Engine.time_scale])
	Net.practice_live = false
	await wait(0.5)

	reset_menu(); Net.active = true; Net.custom_lobby = true; Net.privacy = 0; Net.is_host = false; Net.in_match = true
	await wait(1.0)
	g._on_sync_packet(1, pk.call({"v": 2, "sid": 8, "seq": 1, "ts": 2.0, "params": {"ball.gravity": 40.0}}))
	await frame()
	check(not g.is_physics_allowed() and g.get_sync_status().get("role") == "off" and is_equal_approx(ph.gravity, 11.0) and is_equal_approx(Engine.time_scale, 1.0) and (_rally.players[0] as Player).scale.is_equal_approx(Vector3.ONE), "public lobby in a match: no sync, host packet ignored, stock, time scale 1 (g %.2f ts %.2f)" % [ph.gravity, Engine.time_scale])
	Net.in_match = false
func _goofy_ball(g: Node, ph: Resource, r0: float) -> void:
	var feel = load("res://game/player/player_feel.tres")
	check(g.api_version() == "1.2" and g.is_enabled() and g.is_physics_allowed(), "goofy api: 1.2, enabled, physics allowed in the menu (offline)")
	var schema: Array = g.get_schema()
	check(schema.size() >= 150, "goofy api: schema lists %d params" % schema.size())
	check(is_equal_approx(float(g.get_stock("ball.gravity")), 11.0) and is_equal_approx(float(g.get_stock("player.reach")), 1.15), "goofy api: stock values read from the live resources")
	g.params_changed.connect(func(_c, _e): _gb_changed += 1)
	check(g.validate_config({"ball_params": {"nope.x": 1}}).contains("unknown"), "goofy editor: unknown ball param refused")
	check(g.validate_config({"ball_params": {"ball.gravity": 999}}).contains("between"), "goofy editor: out-of-range ball param refused")
	check(g.validate_config({"ball_params": {"ball.gravity": {"mul": 0.5}, "player.reach": 2}}) == "", "goofy editor: valid ball_params accepted")
	var path := OS.get_environment("JV_MODS_DIR").path_join("goofy").path_join("60-ball.json")
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string('{"ball_params": {"ball.floor_friction": 0.3, "player.reach": {"mul": 2}, "flight.max_speed": 5, "ball.net_half_width": 6, "ball.outline_home": [1, 0, 0, 1]}}'); f.close()
	g.reload_config(); await wait(0.6)
	check(is_equal_approx(ph.floor_friction, 0.3) and is_equal_approx(feel.reach, 2.3), "user ball_params: floor_friction %.2f, reach %.2f (x2)" % [ph.floor_friction, feel.reach])
	check(is_equal_approx(ph.gravity, 11.0 * 0.85) and is_equal_approx(ph.radius, r0 * 1.5), "user ball_params + legacy keys together (gravity %.2f radius %.3f)" % [ph.gravity, ph.radius])
	check(is_equal_approx(gball.net_half_width, 6.0), "live copy: existing ball's net_half_width %.2f" % gball.net_half_width)
	check(ph.outline_home.is_equal_approx(Color(1, 0, 0, 1)), "colour param applied (%s)" % ph.outline_home)
	gball.frozen = false; gball.position = Vector3(0, 6, 0); gball.vel = Vector3(20, 0, 0)
	await get_tree().physics_frame; await get_tree().physics_frame; await get_tree().physics_frame
	gball.frozen = true
	check(gball.vel.length() <= 5.0 + 11.0 * 0.05, "flight.max_speed 5: ball speed capped (%.2f)" % gball.vel.length())
	var r: Dictionary = g.set_override("testmod", {"ball.gravity": 20.0}, 500)
	check(r.get("ok") and is_equal_approx(ph.gravity, 20.0) and _gb_changed > 0, "set_override: mod layer beats user config (gravity %.2f)" % ph.gravity)
	var court: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://goofy_court.json"))
	r = g.apply_pack_params("1090", court["goofy"], int(court["goofy"]["version"]))
	check((r.get("ignored") as Array).has("not.a.param") and (r.get("applied") as Array).size() == 3, "apply_pack_params: 3 applied, unknown key ignored (%s)" % str(r))
	check(g.get_loader_map() == "1090" and is_equal_approx(ph.gravity, 20.0), "pack layer sits under the mod layer (gravity %.2f)" % ph.gravity)
	check(g.clear_override("testmod") and is_equal_approx(ph.gravity, 5.5), "clear_override: court gravity x0.5 now wins over user x0.85 (%.2f)" % ph.gravity)
	check(is_equal_approx(feel.reach, 1.6) and is_equal_approx(ph.floor_friction, 0.3), "per param: court reach 1.6 beats user, user floor_friction kept (%.2f / %.2f)" % [feel.reach, ph.floor_friction])
	check(g.set_ball_mode("fixed") and g.get_force_defaults(), "Ball override FIXED on a loader map -> force_defaults")
	await wait(0.6)
	check(is_equal_approx(ph.gravity, 11.0) and is_equal_approx(ph.radius, r0) and is_equal_approx(feel.reach, 1.15) and is_equal_approx(ph.floor_friction, 0.75), "FIXED: everything stock, user config ignored too (g %.2f r %.3f reach %.2f)" % [ph.gravity, ph.radius, feel.reach])
	check(gball.scale.is_equal_approx(Vector3.ONE) and is_equal_approx(gball.net_half_width, 4.7), "FIXED: ball visual back to stock, net_half_width %.2f" % gball.net_half_width)
	var saved = JSON.parse_string(FileAccess.get_file_as_string(OS.get_environment("JV_MODS_DIR").path_join("goofy/state/ball_override.json")))
	check(saved is Dictionary and saved.get("mode") == "fixed", "FIXED persisted to mods/goofy/state/ball_override.json (%s)" % str(saved))
	check(not g.get_status()["features"].has("_x") and int(g.get_status()["features"]["ball_library"]) == 0, "FIXED: 0 params off stock")
	g.set_loader_map("")
	await wait(0.2)
	check(not g.get_force_defaults() and is_equal_approx(ph.gravity, 11.0 * 0.85) and is_equal_approx(ph.radius, r0 * 1.5), "stock map: FIXED not consulted, user config applies (g %.2f)" % ph.gravity)
	check(g.get_layers().filter(func(l): return str(l["owner"]).begins_with("pack:")).is_empty(), "stock map: the court's stage-scoped layer is gone")
	g.set_ball_mode("normal")
	await _goofy_libs(g, ph, r0)
	await _goofy_host_sync(g, ph, r0)
	reset_menu(); Net.is_host = false
	await wait(1.0)
	check(g.is_physics_allowed() and is_equal_approx(ph.gravity, 11.0 * 0.85) and is_equal_approx(feel.reach, 2.3), "back offline: user ball library re-applied")
	DirAccess.remove_absolute(path); g.reload_config(); await wait(0.6)
	check(is_equal_approx(ph.floor_friction, 0.75) and is_equal_approx(feel.reach, 1.15), "user file removed: library params back to stock")

func expect_gameplay() -> void:
	await wait(1.2)
	check(gate().is_active(), "menu: full tier on (%s)" % gate().get_reason())




func _reset_tilt_transforms() -> void:
	for c in _tilt:
		if c is Control:
			c.offset_transform_rotation = 0.0
			c.offset_transform_scale = Vector2.ONE
			c.offset_transform_position = Vector2.ZERO
			c.z_index = 0
	for c in _lean:
		if c is Control:
			c.offset_transform_rotation = 0.0
			c.offset_transform_scale = Vector2.ONE
			c.offset_transform_position = Vector2.ZERO
			c.z_index = 0

func _snapshot_tilt() -> Array:
	var out: Array = []
	for c in _tilt:
		if c is Control and c.is_visible_in_tree():
			out.append({"rot": c.offset_transform_rotation, "pos": c.offset_transform_position, "scale": c.offset_transform_scale, "z": c.z_index})
	return out

var _tilt_fake := 0.0
var _top_panel: Control = null
var _bottom_panel: Control = null
var _top_strip: Control = null
var _bottom_strip: Control = null
var _use_stock_tilt := false

func _place_strips() -> void:
	if _top_strip != null and _top_panel != null:
		_top_strip.offset_top = _top_panel.size.y
		_top_strip.offset_bottom = _top_panel.size.y + 4.0
	if _bottom_strip != null and _bottom_panel != null:
		_bottom_strip.offset_top = -_bottom_panel.size.y - 4.0
		_bottom_strip.offset_bottom = -_bottom_panel.size.y

func _tilt_cards(delta: float) -> void:

	if _freeplay or _tilt.is_empty():
		return
	var vp: Vector2 = get_viewport().get_visible_rect().size
	if vp.x <= 0.0:
		return
	var mouse: Vector2 = ui.get_viewport().get_mouse_position() if ui != null else get_viewport().get_mouse_position()
	if _fake_mouse.x >= 0.0:
		mouse = _fake_mouse
	var off: Vector2 = (mouse / vp) - Vector2(0.5, 0.5)
	if _tilt_fake != 0.0:
		off.x = _tilt_fake
	var k := clampf(delta * 9.0, 0.0, 1.0)
	var depth := 0
	for c in _tilt:
		var card := c as Control
		if not is_instance_valid(card) or not card.is_visible_in_tree():
			continue
		card.offset_transform_enabled = true
		card.offset_transform_pivot_ratio = Vector2(0.5, 0.5)
		var hovered := card.has_focus() or card.get_global_rect().has_point(mouse)
		var recede := 1.0 - 0.026 * float(depth)
		var fan := 0.028 - 0.016 * float(depth)
		var want_rot := 0.0 if hovered else fan - off.x * 0.06
		var want_scale := 1.07 if hovered else recede - absf(off.x) * 0.012
		var want_x := (52.0 if hovered else 8.0 * float(depth)) - off.x * (26.0 + 8.0 * float(depth))
		card.offset_transform_rotation = lerpf(card.offset_transform_rotation, want_rot, k)
		card.offset_transform_scale = card.offset_transform_scale.lerp(Vector2.ONE * want_scale, k)
		card.offset_transform_position.x = lerpf(card.offset_transform_position.x, want_x, k)
		card.z_index = 2 if hovered else 0
		depth += 1

func _lean_tiles(delta: float) -> void:
	if _freeplay or _lean.is_empty():
		return
	var vp: Vector2 = get_viewport().get_visible_rect().size
	if vp.x <= 0.0:
		return
	var mouse: Vector2 = ui.get_viewport().get_mouse_position() if ui != null else get_viewport().get_mouse_position()
	if _fake_mouse.x >= 0.0:
		mouse = _fake_mouse
	var off: Vector2 = (mouse / vp) - Vector2(0.5, 0.5)
	if _tilt_fake != 0.0:
		off.x = _tilt_fake
	var k := clampf(delta * 9.0, 0.0, 1.0)
	for c in _lean:
		var tile := c as Control
		if not is_instance_valid(tile) or not tile.is_visible_in_tree():
			continue
		tile.offset_transform_enabled = true
		tile.offset_transform_pivot_ratio = Vector2(0.5, 0.5)
		var hovered := tile.get_global_rect().has_point(mouse)
		var want_rot := 0.0 if hovered else -off.x * 0.05
		var want_scale := 1.08 if hovered else 1.0 - absf(off.x) * 0.01
		tile.offset_transform_rotation = lerpf(tile.offset_transform_rotation, want_rot, k)
		tile.offset_transform_scale = tile.offset_transform_scale.lerp(Vector2.ONE * want_scale, k)
		tile.offset_transform_position.x = lerpf(tile.offset_transform_position.x, -off.x * 14.0, k)
		tile.z_index = 2 if hovered else 0

func _tilt_cards_like(_d: float) -> void:
	for c in _tilt:
		if is_instance_valid(c):
			c.set("offset_transform_enabled", true); c.set("offset_transform_rotation", 0.05); if not ("offset_transform_enabled" in c): c.rotation = 0.05


func _card_btn(icon_name: String, title: String, sub: String, kind: String, on_press: Callable, slim := false, _tint := Color(0, 0, 0, 0)) -> Control:
	var b := Button.new()
	b.custom_minimum_size = Vector2(430, 46 if slim else 74)
	var sb := StyleBoxFlat.new(); sb.bg_color = Color(1, 0.8, 0.2) if kind == "primary" else Color(0.95, 0.93, 0.88)
	b.add_theme_stylebox_override("normal", sb)
	var pad := MarginContainer.new(); b.add_child(pad)
	var row := HBoxContainer.new(); pad.add_child(row)
	var st := Control.new(); st.set_meta("icon", icon_name); row.add_child(st)
	var col := VBoxContainer.new(); row.add_child(col)
	var t := Label.new(); t.text = title; col.add_child(t)
	if sub != "":
		var s := Label.new(); s.text = sub; col.add_child(s)
	if on_press.is_valid(): b.pressed.connect(on_press)
	var wrap := MarginContainer.new(); wrap.add_child(b)
	_tilt.append(b)
	return wrap
func _open_credits() -> void: credits_opened += 1

var _guide_page: Control = null
var _feedback: Control = null
var _credits_page: Control = null
var _title_editor: Control = null
var _friends_panel: PanelContainer = null
var _modifiers: Control = null
var _lean: Array = []
var modal_closed_calls := 0
var _menu_live := false
var _fake_mouse := Vector2(-1, -1)
func _modal_up() -> bool:
	return (_feedback != null and is_instance_valid(_feedback)) or (_credits_page != null and is_instance_valid(_credits_page)) \
		or (_guide_page != null and is_instance_valid(_guide_page)) or (_title_editor != null and is_instance_valid(_title_editor))
func _modal_closed() -> void: modal_closed_calls += 1
func _flatten_cards() -> void:
	for c in _tilt:
		if is_instance_valid(c): c.set("offset_transform_enabled", false); c.z_index = 0
func _process(d: float) -> void:
	if not _menu_live or _modal_up(): return
	if _use_stock_tilt:
		_place_strips()
		_tilt_cards(d)
		_lean_tiles(d)
		return
	for c in _tilt:
		if not is_instance_valid(c) or not c.is_visible_in_tree(): continue
		var hovered: bool = c.has_focus() or c.get_global_rect().has_point(_fake_mouse)
		c.set("offset_transform_enabled", true)
		c.z_index = 2 if hovered else 0
var discord_opened := 0

func _tint_button(btn: Button, fill: Color) -> void:
	for st in ["normal", "hover", "pressed", "focus"]:
		var sb := btn.get_theme_stylebox(st)
		if not sb is StyleBoxFlat: continue
		var c := (sb as StyleBoxFlat).duplicate() as StyleBoxFlat
		c.bg_color = fill.lightened(0.08) if st == "hover" or st == "focus" else (fill.darkened(0.08) if st == "pressed" else fill)
		btn.add_theme_stylebox_override(st, c)
func _build_menu_mirror() -> void:
	ui = CanvasLayer.new(); ui.name = "Ui"; add_child(ui)
	_home = Control.new(); ui.add_child(_home)
	var v := VBoxContainer.new(); v.name = "HomeList"; _home.add_child(v)

	var cards := [_card_btn("massiveMultiplayer", "online", "play online", "primary", Callable()),
		_card_btn("singleplayer", "local", "", "info", Callable()),
		_card_btn("target", "practice", "", "info", Callable()),
		_card_btn("gamepad", "controller config", "", "info", Callable()),
		_card_btn("gear", "system settings", "configure audio, video, display", "quiet", Callable()),
		_card_btn("question", "feedback & bug reports", "", "quiet", Callable()),
		_card_btn("movie", "credits", "", "primary", func(): _open_credits(), true)]
	for c in cards: v.add_child(c)
	var bottom := HBoxContainer.new(); bottom.name = "BottomBar"; ui.add_child(bottom); _bottom_bar = bottom
	var pc := Label.new(); pc.text = "player card"; bottom.add_child(pc)

	var discord := Button.new(); discord.text = "discord"; discord.custom_minimum_size = Vector2(0, 40)
	discord.icon = PlaceholderTexture2D.new(); discord.icon.set_meta("icon", "discord_logo")
	var dsb := StyleBoxFlat.new(); dsb.bg_color = Color("5865f2"); discord.add_theme_stylebox_override("normal", dsb)
	discord.pressed.connect(func(): discord_opened += 1)
	bottom.add_child(discord); _discord_btn = discord
	var ver := Label.new(); ver.text = "v1"; bottom.add_child(ver)

func _menuui() -> void:
	reset_menu()
	var mu: Node = gate().get_node_or_null("JVMenuUI")
	check(mu != null, "menu UI node under the gate")
	await wait(0.6)
	check(str(mu.get_info().get("last_skip")) == "home page not built yet" and ui == null, "menu not built yet: skipped, nothing touched (%s)" % mu.get_info())
	_build_menu_mirror()
	var list: VBoxContainer = _home.get_node("HomeList")
	var old_credits: Button = (list.get_child(6) as Control).get_child(0)
	var discord_style_before: StyleBox = _discord_btn.get_theme_stylebox("normal")
	await wait(0.6)
	check(int(mu.get_info().get("injections", 0)) == 1, "menu UI injected once")
	var names := []
	for w in list.get_children():
		if w.is_queued_for_deletion(): continue
		var b0: Button = w.get_child(0)
		names.append("mods" if w.name == "JvmModsCard" else (b0.get_child(0).get_child(0).get_child(1).get_child(0) as Label).text)
	print("  home order: ", names)
	check(names == ["online", "local", "practice", "mods", "controller config", "system settings", "feedback & bug reports"], "home order: mods in controller config's slot, config/settings/feedback shifted down one (%s)" % str(names))
	var mods_btn: Button = list.get_node("JvmModsCard").get_child(0)
	check(mods_btn.get_child(0).get_child(0).get_child(0).get_meta("icon") == "wrench", "mods card built by _card_btn with the 'wrench' icon")
	var homebtn := []
	for w in list.get_children(): if not w.is_queued_for_deletion(): homebtn.append(w.get_child(0))
	var tilt_home := _tilt.filter(func(x): return homebtn.has(x))
	check(tilt_home == homebtn, "_tilt follows the new order (fan depth)")
	check(not is_instance_valid(old_credits) or old_credits.is_queued_for_deletion() or not old_credits.is_inside_tree(), "old rotating credits card removed")
	var credits_btn: Button = _bottom_bar.get_node_or_null("JvmCreditsButton")
	check(credits_btn != null and credits_btn.get_index() == _discord_btn.get_index() + 1, "credits = bottom-bar button right of discord")
	check(credits_btn != null and not _tilt.has(credits_btn) and not credits_btn.get("offset_transform_enabled"), "credits button is stationary (not in _tilt, no offset transform)")
	var csb := credits_btn.get_theme_stylebox("normal") as StyleBoxFlat
	check(csb != null and csb.bg_color.is_equal_approx(UiStyle.PRIMARY) and credits_btn.get_theme_color("font_color").is_equal_approx(UiStyle.INK), "credits button: discord's look, tinted yellow (UiStyle.PRIMARY), ink text")
	check(credits_btn.text == "credits" and credits_btn.icon != null and credits_btn.icon.get_meta("icon") == "movie", "credits button: label + movie icon (%s)" % [credits_btn.icon.get_meta("icon") if credits_btn.icon and credits_btn.icon.has_meta("icon") else credits_btn.icon])
	check(credits_btn.icon != _discord_btn.icon, "credits button does not keep discord's logo")
	check(mods_btn.custom_minimum_size.y >= 74 and mods_btn.custom_minimum_size.y == homebtn[2].custom_minimum_size.y, "mods card is full size like its neighbours (%s vs %s)" % [mods_btn.custom_minimum_size, homebtn[2].custom_minimum_size])
	check(_discord_btn.get_theme_stylebox("normal") == discord_style_before and _discord_btn.text == "discord" and _discord_btn.focus_neighbor_right == NodePath(), "discord itself unchanged")
	var fbk: Button = homebtn[6]
	check(homebtn[2].get_node(homebtn[2].focus_neighbor_bottom) == mods_btn and mods_btn.get_node(mods_btn.focus_neighbor_bottom) == homebtn[4], "focus: practice -> mods -> controller config")
	check(fbk.get_node(fbk.focus_neighbor_bottom) == credits_btn and credits_btn.get_node(credits_btn.focus_neighbor_top) == fbk, "focus: feedback <-> credits")
	check(credits_btn.get_node(credits_btn.focus_neighbor_left) == _discord_btn, "focus: credits left -> discord")
	credits_btn.pressed.emit()
	check(credits_opened == 1 and discord_opened == 0, "credits button opens credits (not discord)")

	var r0 := credits_btn.get_global_rect()
	for i in 10:
		_tilt_cards_like(0.1); await frame()
	check(credits_btn.get_global_rect() == r0 and credits_btn.rotation == 0.0, "credits button does not move with the tilt loop")
	mods_btn.pressed.emit(); await frame(); await frame()
	var panel: Control = ui.get_node_or_null("JvmModsPanel")
	check(panel != null, "mods card opens the mods panel")
	var body: Label = panel.find_child("Body", true, false) if panel else null
	var txt := body.text if body else ""
	print("  panel text:\n", txt)
	check(txt.find("textures: ON") != -1 and txt.find("shots: ON") != -1 and txt.find("main menu") != -1, "panel lists libraries with state + reason")
	check(txt.find("CAFE") != -1 and txt.find("BA11") != -1 and txt.find("cafe skin pack") != -1, "panel lists installed resource packs with IDs")
	var close: Button = panel.find_child("JvmModsClose", true, false) if panel else null
	check(close != null and not _tilt.has(close), "panel close button present (not tilted)")
	mods_btn.pressed.emit(); await frame()
	check(ui.get_children().filter(func(c): return c.name.begins_with("JvmModsPanel")).size() == 1, "second press does not open a second panel")
	if close: close.pressed.emit()
	await frame(); await frame()
	check(ui.get_node_or_null("JvmModsPanel") == null, "close removes the panel")


	_menu_live = true
	var online_btn: Button = homebtn[0]
	_fake_mouse = online_btn.get_global_rect().get_center()
	await frame(); await frame()
	check(online_btn.z_index == 2, "repro baseline: a hovered home card pops (z_index 2) when nothing is open")
	mods_btn.pressed.emit(); await frame(); await frame()
	panel = ui.get_node_or_null("JvmModsPanel")
	check(_modal_up() and _guide_page == panel, "panel open -> the menu's own _modal_up() is true (menu _process / _input stand down)")
	check(panel.mouse_filter == Control.MOUSE_FILTER_STOP and panel.get_node("Backdrop").mouse_filter == Control.MOUSE_FILTER_STOP and panel.z_index >= 10, "full-screen backdrop stops the mouse, panel above the cards")
	for i in 5: await frame()
	check(online_btn.z_index == 0 and not online_btn.get("offset_transform_enabled") and online_btn.mouse_filter == Control.MOUSE_FILTER_IGNORE, "hovered card behind the panel: no pop, no z-raise, no hover")
	online_btn.grab_focus(); await frame(); await frame()
	var fo := get_viewport().gui_get_focus_owner()
	check(fo != null and panel.is_ancestor_of(fo), "focus trapped inside the panel (%s)" % (fo.name if fo else "none"))

	var dvt: Node = get_node_or_null("/root/JVModGate/JVDevTools")
	var dbtn: Button = panel.find_child("JvmDevProfiler", true, false)
	check(dvt != null and dbtn != null and dbtn.text.begins_with("dev tools: frame profiler") and not dvt.profiler_status()["capturing"], "Mods panel has the dev tools frame profiler entry (idle): %s" % (dbtn.text if dbtn else "missing"))
	if dvt and dbtn:
		var c0 := int(dvt.profiler_status()["captures"])
		dbtn.pressed.emit(); await frame()
		check(dvt.profiler_status()["capturing"] and dbtn.text.find("capturing") != -1, "panel entry starts a capture (%s)" % dbtn.text)
		check(is_equal_approx(float(dvt.profiler_status()["seconds"]), 2.0), "panel capture uses profiler_seconds (2 s)")
		dbtn.pressed.emit(); await frame()
		check(not dvt.profiler_status()["capturing"] and int(dvt.profiler_status()["captures"]) == c0 + 1, "pressing again stops it and keeps the capture")
		var ev := InputEventKey.new(); ev.keycode = KEY_F6; ev.pressed = true
		check(not dvt.hotkeys_enabled() and str(dvt.profiler_status()["hotkeys_source"]) == "build" and str(dvt.profiler_status()["profiler_key"]) == "F6", "hotkeys off by default in a normal build (auto -> build), key F6")
		Input.parse_input_event(ev); await frame(); await frame()
		check(not dvt.profiler_status()["capturing"], "F6 does nothing while hotkeys are off")
		dvt.set_hotkeys(true)
		Input.parse_input_event(ev); await frame(); await frame()
		var on_key: bool = dvt.profiler_status()["capturing"]
		await wait(0.4)
		check(on_key and dbtn.text.find("capturing") != -1, "F6 (hotkeys on) starts the same profiler the panel entry shows (%s)" % dbtn.text)
		dbtn.pressed.emit(); await frame()
		check(not dvt.profiler_status()["capturing"] and int(dvt.profiler_status()["captures"]) == c0 + 2, "panel entry stops the capture F6 started")
		Input.parse_input_event(ev); await frame(); await frame()
		check(dvt.profiler_status()["capturing"], "F6 again starts")
		var ev2 := InputEventKey.new(); ev2.keycode = KEY_F6; ev2.pressed = true
		Input.parse_input_event(ev2); await frame(); await frame()
		check(not dvt.profiler_status()["capturing"] and int(dvt.profiler_status()["captures"]) == c0 + 3, "F6 again stops (toggle)")
		dvt.set_hotkeys(false)
		Input.parse_input_event(ev); await frame(); await frame()
		check(not dvt.profiler_status()["capturing"], "F6 inert again after hotkeys off")

	var mu2: Node = mu
	var sidebar: Control = panel.find_child("Sidebar", true, false)
	var main: Node = panel.find_child("Main", true, false)
	check(sidebar != null and main is HBoxContainer and panel.find_child("View", true, false) != null, "mods panel is a left sidebar + right detail layout")
	check(panel.find_child("Mod_goofy", true, false) != null and panel.find_child("Mod_shots", true, false) != null, "list shows each mod as a button")
	check((panel.find_child("Mod_goofy", true, false) as Button).text.to_lower().find("goofy library") != -1, "sidebar uses the Goofy Library display name")
	check(sidebar.is_ancestor_of(panel.find_child("Mod_goofy", true, false)), "mod buttons live in the sidebar")
	var gbtn0: Button = panel.find_child("Mod_goofy", true, false)
	check(gbtn0 != null and not gbtn0.button_pressed, "on open, no sidebar row is selected")
	(panel.find_child("Mod_goofy", true, false) as Button).pressed.emit(); await frame(); await frame()
	check(str(mu2.get_info().get("view")) == "goofy", "clicking a mod opens its detail view")
	var gbtn: Button = panel.find_child("Mod_goofy", true, false)
	check(panel.find_child("Sidebar", true, false) != null and panel.find_child("What", true, false) != null, "selecting a mod keeps the sidebar and shows detail on the right")
	check(gbtn != null and gbtn.button_pressed, "the selected mod is highlighted in the sidebar")
	var what: Label = panel.find_child("What", true, false); var where: Label = panel.find_child("Where", true, false); var stl: Label = panel.find_child("State", true, false)
	check(what != null and what.text.find("Big ball") != -1 and where.text.find("Quick Match") != -1 and stl.text.find("ON") != -1, "detail: what it does, where allowed, current state")
	var gfile := OS.get_environment("JV_MODS_DIR").path_join("goofy").path_join("test.json")
	var before_txt := FileAccess.get_file_as_string(gfile)
	var ts_w: SpinBox = null
	for w in panel.find_children("*", "SpinBox", true, false):
		if w.has_meta("jvm_key") and Array(w.get_meta("jvm_key")) == ["time_scale"]: ts_w = w
	check(ts_w != null and is_equal_approx(ts_w.value, 0.5), "editor: number field for time_scale (0.5)")
	check(panel.find_child("LiveHint", true, false) != null, "detail explains that ported fields apply live")

	var ts_file0 := FileAccess.get_file_as_string(gfile)
	var gs_api = Engine.get_singleton("JVModAPI") if Engine.has_singleton("JVModAPI") else null
	ts_w.value = 0.6
	await frame()
	check(is_equal_approx(Engine.time_scale, 0.6), "ported field applies live without Save (time_scale 0.6)")
	check(FileAccess.get_file_as_string(gfile) == ts_file0, "JSON file unchanged until Save")
	check(gs_api != null and is_equal_approx(float(gs_api.gameplay_get("goofy", "goofy.time_scale")), 0.6), "registry has the live value")

	var raw: TextEdit = null
	for t in panel.find_children("Raw", "TextEdit", true, false):
		if str(t.get_meta("jvm_file")).ends_with("test.json"): raw = t
	raw.visible = true; raw.text = "{ broken"
	var msg: String = mu2.save_detail()
	check(msg.begins_with("not saved") and FileAccess.get_file_as_string(gfile) == before_txt, "invalid JSON rejected, file unchanged (%s)" % msg.split("\n")[0])
	raw.text = raw.get_meta("jvm_raw_orig"); raw.visible = false
	ts_w.value = 0.7
	msg = mu2.save_detail()
	var after = JSON.parse_string(FileAccess.get_file_as_string(gfile))
	check(msg.find("saved test.json") != -1 and after is Dictionary and is_equal_approx(after["time_scale"], 0.7) and after.has("overrides"), "save writes the edited value, keeps the rest (%s)" % msg.replace("\n", " | "))
	check(FileAccess.get_file_as_string(gfile + ".bak") == before_txt and not FileAccess.file_exists(gfile + ".tmp"), "old file kept as test.json.bak, no .tmp left")
	check(msg.find("applied now") != -1 and is_equal_approx(Engine.time_scale, 0.7), "applied live: Engine.time_scale %.2f" % Engine.time_scale)

	var raw2: TextEdit = null
	for t in panel.find_children("Raw", "TextEdit", true, false):
		if str(t.get_meta("jvm_file")).ends_with("test.json"): raw2 = t

	mu2.show_detail("goofy"); await frame()
	for t in panel.find_children("Raw", "TextEdit", true, false):
		if str(t.get_meta("jvm_file")).ends_with("test.json"): raw2 = t
	raw2.visible = true; raw2.text = '{"time_scale": 0.5, "overrides": [{"target":{"group":"hitter"},"set":{"scale":[3,3,3]}}]}'
	msg = mu2.save_detail()
	check(msg.find("saved") != -1 and is_equal_approx(Engine.time_scale, 0.5), "raw JSON edit saves + applies (time_scale back to 0.5)")

	mu2.show_detail("goofy"); await frame()
	for t in panel.find_children("Raw", "TextEdit", true, false):
		if str(t.get_meta("jvm_file")).ends_with("test.json"): raw2 = t
	var keep := FileAccess.get_file_as_string(gfile)
	raw2.visible = true; raw2.text = '{"time_scale": 0.5, "big_ball": 99}'
	msg = mu2.save_detail()
	check(msg.begins_with("not saved") and msg.contains("big_ball must be between") and FileAccess.get_file_as_string(gfile) == keep, "goofy validator: big_ball 99 refused, file unchanged (%s)" % msg.split("\n")[0])
	mu2.show_detail("shots"); await frame()
	var sfile := OS.get_environment("JV_MODS_DIR").path_join("shots").path_join("test.json")
	var skeep := FileAccess.get_file_as_string(sfile)
	var sraw: TextEdit = null
	for t in panel.find_children("Raw", "TextEdit", true, false):
		if str(t.get_meta("jvm_file")).ends_with("test.json"): sraw = t
	sraw.visible = true; sraw.text = '{"shots": {"topspin": {"combo": {"air": "any"}}}}'
	msg = mu2.save_detail()
	check(msg.begins_with("not saved") and msg.contains("airborne") and FileAccess.get_file_as_string(sfile) == skeep, "shots validator: topspin on the ground refused (air-only rule), file unchanged (%s)" % msg.split("\n")[0])
	sraw.text = '{"shots": {"nope": {}}}'
	msg = mu2.save_detail()
	check(msg.begins_with("not saved") and msg.contains("unknown shot"), "shots validator: unknown shot refused (%s)" % msg.split("\n")[0])
	sraw.text = '{"side_spin": {"max_lateral": 7.5}}'
	msg = mu2.save_detail()
	check(msg.find("saved test.json") != -1 and FileAccess.file_exists(sfile + ".bak"), "shots validator: old key side_spin (Curveball alias) still saves (%s)" % msg.replace("\n", " | "))
	check(is_equal_approx(float(shots().get_status().get("curveball_max_lateral", -1)), 7.5), "Curveball alias: old side_spin.max_lateral 7.5 applied live (%s)" % shots().get_status().get("curveball_max_lateral"))
	mu2.show_detail("shots"); await frame()
	for t in panel.find_children("Raw", "TextEdit", true, false):
		if str(t.get_meta("jvm_file")).ends_with("test.json"): sraw = t
	sraw.visible = true; sraw.text = '{"curveball": {"max_lateral": 5.0}, "shots": {"side_spin": {"combo": {"air": "air"}}}}'
	msg = mu2.save_detail()
	check(msg.find("saved test.json") != -1 and is_equal_approx(float(shots().get_status().get("curveball_max_lateral", -1)), 5.0), "Curveball: new key curveball.max_lateral + old shot id side_spin accepted (%s)" % msg.replace("\n", " | "))

	Input.action_press("ui_cancel"); await frame(); Input.action_release("ui_cancel"); await frame()
	check(str(mu2.get_info().get("view")) == "list", "Esc / B in a detail view goes back to the list")
	var gback: Button = panel.find_child("Mod_goofy", true, false)
	check(gback != null and not gback.button_pressed and panel.find_child("Body", true, false) != null, "back to list: sidebar stays, no mod selected, overview on the right")
	Input.action_press("ui_cancel"); await frame(); Input.action_release("ui_cancel"); await frame(); await frame()
	check(ui.get_node_or_null("JvmModsPanel") == null and not _modal_up() and modal_closed_calls >= 1, "Esc / B on the list closes the panel, modal released")
	check(get_viewport().gui_get_focus_owner() == mods_btn, "focus restored to the mods card")
	await frame(); await frame()
	check(online_btn.mouse_filter == Control.MOUSE_FILTER_STOP, "background cards take the mouse again after close")

	_friends_panel = PanelContainer.new(); ui.add_child(_friends_panel)
	get_viewport().gui_release_focus()
	for i in 4: await frame()
	check(online_btn.z_index == 0 and online_btn.mouse_filter == Control.MOUSE_FILTER_IGNORE, "game's friends panel open: background cards do not pop")
	_friends_panel.visible = false
	for i in 4: await frame()
	check(online_btn.z_index == 2 and online_btn.mouse_filter == Control.MOUSE_FILTER_STOP, "friends panel closed: hover works again")
	_friends_panel.queue_free(); _friends_panel = null
	_menu_live = false; _fake_mouse = Vector2(-1, -1)



	Net.active = true; Net.custom_lobby = true; Net.privacy = 0
	await wait(1.0)
	check(list.get_node_or_null("JvmModsCard") != null and credits_btn.get_parent() == _bottom_bar, "gate off: menu UI stays")
	mods_btn.pressed.emit(); await frame(); await frame()
	panel = ui.get_node_or_null("JvmModsPanel")
	txt = (panel.find_child("Body", true, false) as Label).text if panel else ""
	check(txt.find("shots: BLOCKED") != -1, "panel shows blocked libraries with the gate reason")
	mu.show_detail("shots"); await frame()
	var sv: Button = panel.find_child("JvmSave", true, false) if panel else null
	var lk: Label = panel.find_child("Status", true, false) if panel else null
	check(sv != null and sv.disabled and lk.text.begins_with("settings locked here"), "public lobby: shots settings locked (%s)" % (lk.text if lk else "?"))
	mu.show_detail("textures"); await frame()
	sv = panel.find_child("JvmSave", true, false)
	check(sv != null and not sv.disabled, "public lobby: resource-pack (cosmetic) settings still editable")
	if panel: (panel.find_child("JvmModsClose", true, false) as Button).pressed.emit()
	reset_menu()

	_discord_btn.visible = false; await wait(0.4)
	check(not credits_btn.visible, "credits hides with discord (matchmaking)")
	_discord_btn.visible = true; await wait(0.4)
	check(credits_btn.visible, "credits back with discord")
	await wait(0.6)
	var mods_cards := list.get_children().filter(func(c): return c.name.begins_with("JvmModsCard")).size()
	var credit_btns := _bottom_bar.get_children().filter(func(c): return c.name.begins_with("JvmCreditsButton")).size()
	check(mods_cards == 1 and credit_btns == 1 and int(mu.get_info().get("injections")) == 1, "no duplicate cards over time")
	await _accessory_split()


func _acc_by_id(st: Dictionary, id: String) -> Dictionary:
	for e in st.get("entries", []):
		if e.get("id") == id: return e
	return {}
func _accessory_split() -> void:
	var T := OS.get_environment("JV_MODS_DIR").path_join("resources/packs")
	if not DirAccess.dir_exists_absolute(T.path_join("champion_cap")):
		print("SKIP accessory split (fixture packs not present)")
		return

	GameConfig.character.accessories = {}
	_wjson(T.path_join("equipped.json"), {"equipped": ["CAFE", "A100", "A103", "A10F"]})
	tl().reload_config()
	await frame(); await frame(); await frame()
	var st: Dictionary = tl().get_accessory_status()
	check(bool(st.get("injected", false)), "locker accessories injected into AccessoryCatalog (%s)" % str(st))
	check(_acc_by_id(st, "A100").get("category") == "head", "Champion Cap is a HEAD accessory")
	check(_acc_by_id(st, "A10F").get("category") == "face", "Custom Face is a FACE accessory")
	check(_acc_by_id(st, "A103").get("category") == "extra", "Flame Tail is an EXTRA accessory")
	check(AccessoryCatalog.for_category("head").any(func(it): return it.id == "A100"), "catalog HEAD lists Champion Cap independently")
	check(AccessoryCatalog.for_category("face").any(func(it): return it.id == "A10F"), "catalog FACE lists Custom Face independently")
	check(AccessoryCatalog.for_category("extra").any(func(it): return it.id == "A103"), "catalog EXTRA lists Flame Tail independently")
	var ch = GameConfig.character
	check(ch.acc_id("head") == "A100" and ch.acc_id("face") == "A10F" and ch.acc_id("extra") == "A103", "equipped.json accessories migrated into empty locker slots (head=%s face=%s extra=%s)" % [ch.acc_id("head"), ch.acc_id("face"), ch.acc_id("extra")])
	var eq: Array = tl().get_sync_status().get("equipped", [])
	check(eq.has("CAFE") and eq.has("A100") and eq.has("A103") and eq.has("A10F"), "outfit CAFE and the three accessories all stay in equipped.json after migrate %s" % str(eq))

	ch.accessories = {"head": {"id": "baseballcap"}}
	_wjson(T.path_join("equipped.json"), {"equipped": ["CAFE", "A100"]})
	tl().reload_config(); await frame(); await frame(); await frame()
	check(ch.acc_id("head") == "baseballcap" and ch.acc_id("head2") == "A100", "migration never overwrites an existing locker pick (head=%s head2=%s)" % [ch.acc_id("head"), ch.acc_id("head2")])

	ch.set_acc_id("head2", "")
	await wait(0.7)
	eq = tl().get_sync_status().get("equipped", [])
	check(eq.has("CAFE") and not eq.has("A100"), "unequipping the cap removes it from equipped.json and keeps the outfit %s" % str(eq))

	GameConfig.character.accessories = {}
	_wjson(T.path_join("equipped.json"), {"equipped": ["CAFE"]})
	tl().reload_config(); await frame(); await frame()


func ml() -> Node: return get_node_or_null("/root/JVModelLoader")
var _other_seen := [0]
func _other_hook(sc: Node) -> void: _other_seen[0] += 1; sc.set_meta("other_hook", true)
var _hand_shader: Shader = null
func mk_rigged(nm: String, who: String, under: Node = null) -> ModelRig:

	var parent: Node = self
	var vis := Node3D.new(); vis.name = "Visual"
	if who != "none":
		var p := Player.new(); p.name = nm; (under if under else self).add_child(p); p.add_child(vis)
		if who == "local": player = p
		elif who == "remote": _peer_players[2] = p
	else:
		var holder := Node3D.new(); holder.name = nm; add_child(holder); holder.add_child(vis)
	if _hand_shader == null:
		_hand_shader = Shader.new(); _hand_shader.code = "shader_type spatial;\nuniform vec4 albedo : source_color = vec4(1.0);\nvoid fragment(){ ALBEDO = albedo.rgb; }"
	for h in ["HandL", "HandR"]:
		var mi := MeshInstance3D.new(); mi.name = h; mi.mesh = SphereMesh.new()
		var sm := ShaderMaterial.new(); sm.shader = _hand_shader; sm.set_shader_parameter("albedo", Color(1.0, 0.87, 0.73)); mi.material_override = sm
		vis.add_child(mi)
	var rig := ModelRig.new(); rig.name = "ModelRig"; vis.add_child(rig)
	rig.setup()
	return rig
func grafts(rig: ModelRig) -> Array:
	var out := []
	if rig._skel: for c in rig._skel.get_children(): if c.has_meta("jvm_model"): out.append(c)
	return out
func graft_nodes_under(n: Node) -> Array:
	var out := []
	if n == null: return out
	if n.has_meta("jvm_model"): out.append(n)
	for c in n.get_children(): out.append_array(graft_nodes_under(c))
	return out
func base_hidden(rig: ModelRig) -> bool:
	for n in ["bodymesh", "jerseymesh", "headmesh"]:
		var m: Node3D = rig._skel.get_node_or_null(n)
		if m == null or m.visible: return false
	for s in rig._shells.values(): if s.visible: return false
	return not (rig._face_root.get_child(0) as Label3D).visible
func base_visible(rig: ModelRig) -> bool:
	for n in ["bodymesh", "jerseymesh", "headmesh"]:
		var m: Node3D = rig._skel.get_node_or_null(n)
		if m == null or not m.visible: return false
	return (rig._face_root.get_child(0) as Label3D).visible and not rig._skel.get_node("handsmesh").visible
func gmat(g: Node) -> Material:
	var mi := g as MeshInstance3D
	return mi.material_override if mi.material_override else mi.get_surface_override_material(0)


func mroot_rel() -> String:
	var r := OS.get_environment("JV_MODEL_ROOT")
	return r if r != "" else "models"
func merged_layout() -> bool: return mroot_rel() != "models"
func mroot() -> String: return OS.get_environment("JV_MODS_DIR").path_join(mroot_rel())
func equip(model: String, extra := []) -> void:
	var path := mroot().path_join("equipped.json")
	var d := {"equipped": extra, "model": model}
	if merged_layout():
		var old = JSON.parse_string(FileAccess.get_file_as_string(path)) if FileAccess.file_exists(path) else null
		d = old if old is Dictionary else {}
		d["model"] = model
		if not extra.is_empty(): d["equipped"] = extra
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string(JSON.stringify(d)); f.close()
	if not merged_layout():
		var mp := OS.get_environment("JV_MODS_DIR").path_join("resources/packs/equipped.json")
		var md = JSON.parse_string(FileAccess.get_file_as_string(mp)) if FileAccess.file_exists(mp) else null
		if md is Dictionary and md.has("model"):
			md.erase("model"); var g := FileAccess.open(mp, FileAccess.WRITE); g.store_string(JSON.stringify(md)); g.close()
	ml().reload_config()
	await frame(); await frame(); await frame()
func log_text() -> String: return FileAccess.get_file_as_string("user://jv_mods/jv_mods.log")
func mstatus() -> Dictionary: return ml().get_status()

var remote_rig: ModelRig = null
func _models() -> void:
	await wait(0.9)
	check(gate().is_cosmetic_active(), "menu: cosmetic tier open")
	var st0 := mstatus()
	check(st0.get("state") == "active" and st0.get("tier") == "cosmetic", "models library active on the COSMETIC tier (%s)" % str(st0.get("state")))
	check(st0.get("hook") == "installed", "ModelRig.model_hook installed (%s)" % st0.get("hook"))
	check(int(st0.get("packs", 0)) >= 9, "9+ model packs registered (%s)" % st0.get("packs"))
	check(str(st0.get("model", "")).begins_with("ready: AE01"), "AE01 glb template ready (%s)" % st0.get("model"))
	var w := "\n".join(st0.get("warnings", []))
	check(w.find("E5C0") != -1 or w.find("escape") != -1, "pack with '..' path rejected: " + w.substr(0, 120))
	check(w.find("AE02 is a model pack listed under \"equipped\"") != -1, "model id under \"equipped\" warned, not used")
	check(tl().get_sync_status().get("equipped", []).has("CAFE"), "texture skins unaffected (CAFE still equipped)")

	var remote := mk_rigged("Remote", "remote")
	remote_rig = remote
	var bot := mk_rigged("Bot", "bot")
	var rig := mk_rigged("Me", "local")
	await frame(); await frame()
	var g := grafts(rig)
	check(g.size() == 1, "local player: model grafted onto the game's Skeleton3D (%d part)" % g.size())
	check(grafts(remote).is_empty() and base_visible(remote), "remote player keeps the game's model (apply_to local)")
	check(grafts(bot).is_empty(), "unknown/bot player keeps the game's model")
	if g.is_empty(): finish(); return
	var mi := g[0] as MeshInstance3D
	check(mi.get_parent() == rig._skel and mi.skeleton == NodePath("..") and mi.skin != null, "skinned part under Armature/Skeleton3D, skeleton '..'")
	var named := true
	for b in mi.skin.get_bind_count(): named = named and rig._skel.find_bone(mi.skin.get_bind_name(b)) >= 0
	check(mi.skin.get_bind_count() == 26 and named, "26 skin binds re-bound by bone NAME to the game skeleton")
	check(not String(mi.name).to_lower().contains("hand") and not String(mi.name).to_lower().contains("head"), "graft name avoids ModelRig name rules (%s)" % mi.name)
	check(base_hidden(rig), "base meshes, ink shells and face hidden")
	check(is_equal_approx(rig.hand_radius, remote.hand_radius) and rig.hand_rest.is_equal_approx(remote.hand_rest) and is_equal_approx(rig._base_scale, remote._base_scale),
		"hand_radius/hand_rest/TARGET_HEIGHT scale identical to vanilla (%.4f %s %.4f)" % [rig.hand_radius, rig.hand_rest, rig._base_scale])
	check(rig.hands_animated(), "hand bones still found (hand reach)")

	var arm := rig._skel.find_bone("arm.r")
	var before := rig._skel.get_bone_global_pose(arm)
	rig.play_clip("spike"); await wait(0.6)
	check(not rig._skel.get_bone_global_pose(arm).is_equal_approx(before) and mi.get_parent() == rig._skel, "game AnimationPlayer moves the bones the model is skinned to")

	var m := gmat(mi) as StandardMaterial3D
	var own: Color = m.albedo_color if m else Color.BLACK
	rig.set_slot_color("jersey", Color(1, 0, 0)); await frame(); await frame()
	check(m != null and m.albedo_color.is_equal_approx(own), "team tint disabled: jersey colour ignored, model keeps its own albedo (%s)" % (m.albedo_color if m else "null"))
	var hl := rig.get_parent().get_node("HandL") as MeshInstance3D
	check((hl.material_override as ShaderMaterial).get_shader_parameter("albedo").is_equal_approx(Color(own, 1)), "game_hands tint: hand spheres use the model's own colour, not the jersey")
	check(gmat(grafts(rig)[0]) != gmat(grafts(remote)[0]) if not grafts(remote).is_empty() else true, "materials are per player copies")

	rig.set_head_visible(true); await frame(); await frame()
	check(base_hidden(rig), "game re-showed head/face -> re-hidden next frame")

	await wait(0.6)
	var ms: Dictionary = status().get("libraries", {}).get("models", {})
	check(ms.get("state") == "active" and int(ms.get("applied", 0)) == 1 and ms.get("equipped", {}).get("id") == "AE01", "status.json libraries.models active, applied 1, equipped AE01")

	set_lobby("custom", -1); await frame(); await frame()
	check(grafts(rig).is_empty() and base_visible(rig), "unknown context: model removed, game's model restored (fail closed)")
	check((hl.material_override as ShaderMaterial).get_shader_parameter("albedo").is_equal_approx(rig.skin_color()), "hands back to skin colour")
	set_lobby("custom", 2); await wait(0.9); await frame()
	check(grafts(rig).size() == 1 and base_hidden(rig), "public lobby (cosmetic tier): model back on, local only")
	set_lobby("offline", -1); await wait(0.9)

	await equip("AE02")
	g = grafts(rig)
	check(g.size() == 1, "AE02 (second .glb) grafted")
	check(grafts(remote).is_empty(), "remote still vanilla after reload")

	await equip("F1C0")
	g = grafts(rig)
	check(g.size() == 1 and (gmat(g[0]) as StandardMaterial3D).albedo_color.is_equal_approx(Color("#FF1A8C")), "\"color\" pack: fixed colour, no team tint")

	var why_cases := [["BAD1", "cannot read"], ["B0E5", "skeleton mismatch"], ["5C01", "self-contained .glb"], ["5C02", "self-contained .glb"], ["AE03", "self-contained .glb"], ["E7B0", "has a uri"], ["4155", "file not found"], ["E5C0", "invalid skin.json"], ["CAFE", "not installed"], ["0000", "not installed"]]
	if not merged_layout(): why_cases.append(["5C1A", "not a model pack"])
	for c in why_cases:
		await equip(c[0])
		var s2 := mstatus()
		var txt := str(s2.get("model")) + " " + str(s2.get("last_error", "")) + " " + "\n".join(s2.get("warnings", [])) + " " + str(ml().get_rig_report())
		check(grafts(rig).is_empty() and base_visible(rig), "%s: falls back to the game's model" % c[0])
		check(txt.find(c[1]) != -1 or log_text().find(c[1]) != -1, "%s: reason reported ('%s')" % [c[0], c[1]])
	check(log_text().find("[jv-mods:models]") != -1, "jv_mods.log has [jv-mods:models] lines")

	check(int(Engine.get_meta("jvm_pwned", 0)) == 0, "GLB with an external image URI to evil.tres: refused, canary never ran")
	check(not get_tree().root.has_meta("jvm_script_ran") and not log_text().contains("JVM_SCRIPT_RAN"), "allow_scripts pack: refused, its script never ran")

	await equip("AE01")
	var seen := _other_seen
	ModelRig.model_hook = Callable(self, "_other_hook")
	await wait(1.3)
	var rig2 := mk_rigged("Presenter", "none")
	await frame(); await frame()
	check(seen[0] == 1 and rig2._scene.has_meta("other_hook") and grafts(rig2).size() == 1, "foreign hook chained (called %d) and our graft still applied (no Player = your presentation)" % seen[0])

	var rig3 := mk_rigged("Mirror", "none")
	for n in rig3.find_children("*", "VisualInstance3D", true, false): if not n.has_meta("jvm_model"): n.layers = 1 << 10
	ml().reload_config(); await frame(); await frame(); await frame()
	check(grafts(rig3).size() == 1 and (grafts(rig3)[0] as VisualInstance3D).layers == 1 << 10, "graft copies the game meshes' render layers (reflections)")

	var n0: int = int(mstatus().get("rigs", 0))
	rig3.get_parent().get_parent().queue_free(); await frame(); await frame(); await frame()
	check(int(mstatus().get("rigs", 0)) == n0 - 1, "freed rig untracked")
	await _reflection_copy(rig)
	await _folders(rig)
	await _sit_accessories()
	await _locker_accessory_visibility()
	await _locker_tab()
	gate().queue_free(); await frame(); await frame()
	check(grafts(rig).is_empty() and base_visible(rig), "core gate removed -> models revert (fail closed)")




func _reflection_copy(rig: ModelRig) -> void:
	var pl: Node = rig.get_parent().get_parent()
	var holder := Node3D.new(); holder.top_level = true; pl.add_child(holder); pl.set("_reflect_root", holder)
	var refl := ModelRig.new(); holder.add_child(refl); refl.setup(true)
	ModelRig.hand_local_bad = 0
	await frame(); await frame(); await wait(0.7)
	check(refl._reflection and refl._bone["l"] == -1, "reflection copy built like the game (setup(true), hands not calibrated)")
	check(grafts(refl).is_empty() and graft_nodes_under(holder).is_empty(), "reflection copy keeps the game's model (never grafted)")
	var g := grafts(rig)
	check(g.size() == 1 and (g[0] as Node3D).visible and (g[0] as VisualInstance3D).layers != 0, "player with a reflection copy: the real graft stays visible (layers %d)" % ((g[0] as VisualInstance3D).layers if g.size() else -1))
	check(base_hidden(rig), "player with a reflection copy: stock body still hidden on the real rig")
	if g.size():
		(g[0] as Node3D).visible = false; (g[0] as Node3D).visible = true; await frame(); await frame()
		check((g[0] as Node3D).visible and (g[0] as VisualInstance3D).layers != 0, "graft flipped hidden->shown is not re-hidden by the stock visibility hook (layers %d)" % (g[0] as VisualInstance3D).layers)

	await equip("BEA1"); await wait(0.7)
	var arms := graft_nodes_under(pl).filter(func(n): return String(n.name).begins_with("JvmArm"))
	check(arms.size() == 10 and arms.all(func(n): return not holder.is_ancestor_of(n)), "BEA1 arms: 10 JvmArm parts, none on the reflection copy (%d)" % arms.size())
	check(arms.all(func(n): return (n as Node3D).visible and (n as VisualInstance3D).layers != 0), "BEA1 arms visible with a reflection copy present")
	check(grafts(rig).size() >= 1 and (grafts(rig)[0] as Node3D).visible and grafts(refl).is_empty(), "BEA1 body visible on the real rig, reflection copy untouched")

	var nh := mk_rigged("NoHands", "none")
	for h in ["HandL", "HandR"]: nh.get_parent().get_node(h).free()
	nh._bone = {"l": -1, "r": -1}
	await frame(); await wait(0.7)
	check(grafts(nh).size() >= 1, "uncalibrated-hands rig still wears the model")
	check(ModelRig.hand_local_bad == 0, "hand_local never called on a rig without calibrated hands (p_bone -1 calls: %d)" % ModelRig.hand_local_bad)
	nh.get_parent().get_parent().queue_free()
	pl.set("_reflect_root", null); holder.queue_free()
	await equip("AE01"); await wait(0.6)
	check(grafts(rig).size() == 1 and (grafts(rig)[0] as Node3D).visible, "back to AE01 after the reflection test")


func _cp_dir(from: String, to: String) -> void:
	DirAccess.make_dir_recursive_absolute(to)
	for f in DirAccess.get_files_at(from): DirAccess.copy_absolute(from.path_join(f), to.path_join(f))
	for d in DirAccess.get_directories_at(from): _cp_dir(from.path_join(d), to.path_join(d))
func _rm_dir(p: String) -> void:
	for f in DirAccess.get_files_at(p): DirAccess.remove_absolute(p.path_join(f))
	for d in DirAccess.get_directories_at(p): _rm_dir(p.path_join(d))
	DirAccess.remove_absolute(p)
func _wjson(path: String, v) -> void:
	var f := FileAccess.open(path, FileAccess.WRITE); f.store_string(JSON.stringify(v)); f.close()
func _treload() -> void:
	tl().reload_config(); await frame(); await frame(); await frame()

func _weq(T: String, ids: Array) -> void:
	var p := T.path_join("equipped.json")
	var old = JSON.parse_string(FileAccess.get_file_as_string(p)) if FileAccess.file_exists(p) else null
	var d: Dictionary = old if old is Dictionary else {}
	d["equipped"] = ids
	_wjson(p, d)



func _merged_folder_checks(rig: ModelRig, M: String, T: String) -> void:
	check(tl().pack_info("AE01").is_empty() and tl().pack_info("AE02").is_empty() and not tl().pack_info("CAFE").is_empty(), "merged: model packs in resources/packs are not texture packs")
	check(not tl().get_sync_status().get("equipped", []).has("AE02"), "merged: model id never sent as a skin")
	check(mstatus().get("legacy_models_folder") == false and mstatus().get("content_folder") == "resources/packs", "merged: status reports the one folder")
	var eq = JSON.parse_string(FileAccess.get_file_as_string(T.path_join("equipped.json")))
	check(eq is Dictionary and eq.get("model") == "AE01" and Array(eq.get("equipped", [])).has("CAFE"), "merged: equipped.json keeps skins + model pick together (%s)" % str(eq))

	var OM := M.path_join("models")
	_cp_dir(T.path_join("aero glb"), OM.path_join("aero glb"))
	_cp_dir(T.path_join("aero glb"), OM.path_join("old only"))
	var sj = JSON.parse_string(FileAccess.get_file_as_string(OM.path_join("old only/skin.json")))
	sj["id"] = "AE0D"; sj["name"] = "old folder bean"; _wjson(OM.path_join("old only/skin.json"), sj)
	await equip("AE0D")
	check(grafts(rig).size() == 1 and str(mstatus().get("model", "")).begins_with("ready: AE0D") and mstatus().get("legacy_models_folder") == true, "merged: pack only in the old mods/models still worn (%s)" % mstatus().get("model"))
	await equip("AE01")
	var e: Dictionary = mstatus().get("equipped", {}) if mstatus().get("equipped") is Dictionary else {}
	check(grafts(rig).size() == 1 and e.get("pack") == "aero glb", "merged: same id in both folders -> the resources/packs copy (%s)" % str(e.get("pack")))

	_wjson(OM.path_join("equipped.json"), {"model": "AE0D"})
	ml().reload_config(); await frame(); await frame(); await frame()
	check(str(mstatus().get("model", "")).begins_with("ready: AE01"), "merged: merged pick wins over models/equipped.json")
	var d: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(T.path_join("equipped.json")))
	d.erase("model"); _wjson(T.path_join("equipped.json"), d)
	ml().reload_config(); await frame(); await frame(); await frame()
	check(str(mstatus().get("model", "")).begins_with("ready: AE0D"), "merged: no pick in resources/packs -> old models/equipped.json read (fallback)")
	_rm_dir(OM)
	await equip("AE01")
	check(grafts(rig).size() == 1 and mstatus().get("legacy_models_folder") == false, "merged: old folder removed, merged pick still worn")
	check(tl().get_swapped_count() == 1 and tl().get_sync_status().get("equipped", []).has("CAFE"), "merged: textures unaffected by the model checks")

func _folders(rig: ModelRig) -> void:
	var M := OS.get_environment("JV_MODS_DIR")
	var MD := mroot(); var T := M.path_join("resources/packs")

	_cp_dir(MD.path_join("aero glb"), MD.path_join("aero copy renamed"))
	var sj = JSON.parse_string(FileAccess.get_file_as_string(MD.path_join("aero copy renamed/skin.json")))
	sj["id"] = "AE0C"; sj["name"] = "copied bean"; _wjson(MD.path_join("aero copy renamed/skin.json"), sj)
	await equip("AE0C")
	check(grafts(rig).size() == 1 and str(mstatus().get("model", "")).begins_with("ready: AE0C"), "models: copied/renamed folder found at runtime and worn (%s)" % mstatus().get("model"))
	await equip("AE01")
	check(grafts(rig).size() == 1 and str(mstatus().get("model", "")).begins_with("ready: AE01"), "models: swap back to AE01 via models/equipped.json")
	await equip("AE0C")
	_rm_dir(MD.path_join("aero copy renamed")); ml().reload_config(); await frame(); await frame(); await frame()
	check(grafts(rig).is_empty() and base_visible(rig), "models: equipped folder deleted -> game's own model, no crash")

	DirAccess.rename_absolute(M.path_join("resources"), M.path_join("resources_off")); await _treload()
	if merged_layout():
		ml().reload_config(); await frame(); await frame(); await frame()
		check(grafts(rig).is_empty() and base_visible(rig), "models (merged): mods/resources missing -> game's model, no crash")
	else:
		await equip("AE01")
		check(grafts(rig).size() == 1, "models: work with mods/resources missing")
	check(int(tl().get_status().get("packs", -1)) == 0 and tl().get_swapped_count() == 0, "textures: folder missing -> 0 packs, vanilla, no crash")

	DirAccess.rename_absolute(M.path_join("resources_off/packs"), M.path_join("resource packs")); await _treload()
	check(int(tl().get_status().get("packs", 0)) >= 2 and tl().get_swapped_count() == 1, "textures: old 'mods/resource packs' read as fallback (%s)" % str(tl().get_status().get("packs")))
	DirAccess.rename_absolute(M.path_join("resource packs"), M.path_join("resources_off/packs"))
	DirAccess.rename_absolute(M.path_join("resources_off"), M.path_join("resources")); await _treload()
	if merged_layout():
		await equip("AE01")
		check(grafts(rig).size() == 1, "models (merged): back with mods/resources")
	var n0 := int(tl().get_status().get("packs", 0))
	check(n0 >= 2 and tl().get_swapped_count() == 1, "textures: back in resources/packs (%d packs)" % n0)

	_cp_dir(T.path_join("cafe skin pack"), T.path_join("cafe renamed"))
	_wjson(T.path_join("cafe renamed/skin.json"), {"id": "C0FE", "name": "cafe renamed", "kind": "skin"})
	_weq(T, ["CAFE", "C0FE"]); await _treload()
	check(int(tl().get_status().get("packs", 0)) == n0 + 1 and tl().get_sync_status().get("equipped", []).has("C0FE"), "textures: new folder found at runtime and equippable")
	DirAccess.rename_absolute(T.path_join("ballpack"), M.path_join("ballpack_off")); DirAccess.rename_absolute(T.path_join("dupe"), M.path_join("dupe_off")); _rm_dir(T.path_join("cafe renamed")); await _treload()
	check(tl().get_swapped_count() == 0 and not tl().get_sync_status().get("equipped", []).has("C0FE"), "textures: removed packs -> vanilla ball, missing equipped id skipped (swaps %d, eq %s, dirs %s)" % [tl().get_swapped_count(), str(tl().get_sync_status().get("equipped")), str(DirAccess.get_directories_at(T))])
	DirAccess.rename_absolute(M.path_join("ballpack_off"), T.path_join("ballpack"))
	DirAccess.rename_absolute(M.path_join("dupe_off"), T.path_join("dupe"))
	_weq(T, ["CAFE"]); await _treload()
	check(tl().get_swapped_count() == 1, "textures: pack restored")
	if merged_layout():
		await _merged_folder_checks(rig, M, T)
	else:

		DirAccess.rename_absolute(MD, M.path_join("models_off")); ml().reload_config(); await frame(); await frame(); await frame()
		check(grafts(rig).is_empty() and base_visible(rig) and tl().get_swapped_count() == 1, "mods/models missing -> game's model; textures unaffected")
		DirAccess.rename_absolute(M.path_join("models_off"), MD)
		await equip("AE01")
		check(grafts(rig).size() == 1, "models folder restored")
	await _accessory_split()

	var lkacc: Control = load("res://game/character/locker.gd").new(); lkacc.name = "LockerAcc"; add_child(lkacc)
	lkacc._select_section("accessories"); await frame()
	for cat_id in ["head", "face", "extra"]:
		lkacc._select_sub(cat_id); await frame()
		var labels: Array = []
		for c in lkacc._content.get_children():
			if c is Button: labels.append((c as Button).text)
		var want: String = {"head": "Champion Cap", "face": "Custom Face", "extra": "Flame Tail"}[cat_id]
		check(labels.has(want), "locker accessories/%s lists %s independently %s" % [cat_id, want, str(labels)])
	lkacc.queue_free()


func _find(n: Node, pred: Callable) -> Array:
	var out := []
	for c in n.find_children("*", "", true, false): if pred.call(c): out.append(c)
	return out
func _btn(root: Node, text: String) -> Button:
	var a := _find(root, func(c): return c is Button and (c as Button).text == text)
	return a[0] if a.size() else null
func cfolder() -> String: return OS.get_environment("JV_MODS_DIR").path_join("resources/packs")
func _equipped_model() -> String:
	var v = JSON.parse_string(FileAccess.get_file_as_string(cfolder().path_join("equipped.json")))
	return str(v.get("model", "")) if v is Dictionary else "?"
func _settings_file() -> Dictionary:
	var v = JSON.parse_string(FileAccess.get_file_as_string(cfolder().path_join("model_settings.json")))
	return v.get("models", {}) if v is Dictionary else {}
func _seat_of(n: Node) -> Node:
	var p := n.get_parent()
	return p if p != null and p.has_meta("jvm_anchor_seat") else null
func _anchor_of(n: Node) -> BoneAttachment3D:
	var s := _seat_of(n)
	return s.get_parent() as BoneAttachment3D if s != null else null
func _sit_accessories() -> void:


	var rig := mk_rigged("AnchorPlayer", "bot")
	var p: Node3D = rig.get_parent().get_parent()
	var sk: Skeleton3D = rig._skel
	var head_b := sk.find_bone("head")
	var spine1_b := sk.find_bone("spine1")

	var hm := BoneAttachment3D.new(); hm.bone_idx = head_b; sk.add_child(hm)
	var cm := BoneAttachment3D.new(); cm.name = "Chest Mount"; cm.bone_idx = spine1_b; sk.add_child(cm)
	var stock_top := -1e9
	for mi in sk.get_children():
		if mi is MeshInstance3D and String(mi.name).begins_with("headmesh"):
			stock_top = maxf(stock_top, (mi as MeshInstance3D).transform.origin.y + (mi as MeshInstance3D).get_aabb().end.y)
	var m := sk.global_transform.basis.get_scale().y

	var place_cap := Transform3D(Basis(), Vector3(0, stock_top - 0.06 / m, 0))
	var place_tail := Transform3D(Basis(), Vector3(0, 1.4, -0.3))
	var cap := _make_catalog_acc("A100", hm); cap.transform = sk.get_bone_global_rest(head_b).affine_inverse() * place_cap
	var cap2 := _make_catalog_acc("A100", hm); cap2.name = "JvmAcc_A100_2"; cap2.transform = cap.transform
	var tail := _make_catalog_acc("A103", cm); tail.transform = sk.get_bone_global_rest(spine1_b).affine_inverse() * place_tail
	var tail2 := _make_catalog_acc("A103", cm); tail2.name = "JvmAcc_A103_2"; tail2.transform = tail.transform
	rig._acc_roots = {"head": cap, "head2": cap2, "extra": tail, "extra2": tail2}
	rig._acc_ids = {"head": "A100", "head2": "A100", "extra": "A103", "extra2": "A103"}

	var cap_local := cap.transform
	var cap_y0 := cap.global_position.y
	var tail_y0 := tail.global_position.y
	var cap_scale0 := cap.global_transform.basis.get_scale().x
	await frame()
	check(_anchor_of(cap) != null and _anchor_of(tail) != null, "anchors: game-mounted roots adopted by the node_added hook without an explicit sync")
	tl().pose_game_accessory_roots(p)
	await frame()
	var ah := sk.get_node_or_null("JVM_Anchor_head") as BoneAttachment3D
	var ab := sk.get_node_or_null("JVM_Anchor_back") as BoneAttachment3D
	check(ah != null and ah.bone_idx == head_b, "anchors: JVM_Anchor_head is a BoneAttachment3D on the rig skeleton's head bone")
	check(ab != null and ab.bone_idx == spine1_b, "anchors: JVM_Anchor_back is on the chest bone (spine1, same as the game's Chest Mount)")
	check(_anchor_of(cap) == ah and _anchor_of(cap2) == ah, "anchors: both hats sit under JVM_Anchor_head")
	check(_anchor_of(tail) == ab and _anchor_of(tail2) == ab, "anchors: both tails sit under JVM_Anchor_back (slot class from the game's mount, not the node name)")
	check(_seat_of(cap) != _seat_of(cap2) and _seat_of(tail) != _seat_of(tail2), "anchors: one seat per instance (duplicate item ids do not share an owner)")
	check(cap.transform.is_equal_approx(cap_local), "anchors: the game's own local transform on the root is kept (no global writes)")
	check(abs(cap.global_transform.basis.get_scale().x / cap_scale0 - 0.9) < 0.01, "anchors: hat scaled to 0.90 (%s)" % (cap.global_transform.basis.get_scale().x / cap_scale0))

	var torso := AABB(); var has_t := false
	for mi in sk.get_children():
		if mi is MeshInstance3D and (String(mi.name) == "bodymesh" or String(mi.name).contains("jersey")) and not String(mi.name).ends_with("_shell"):
			var b: AABB = (mi as MeshInstance3D).transform * (mi as MeshInstance3D).get_aabb()
			torso = b if not has_t else torso.merge(b); has_t = true
	var want_back := Vector3(torso.get_center().x, torso.position.y + torso.size.y * 0.55 - 0.05 / m, torso.position.z)
	var tail_rest: Vector3 = (sk.get_bone_global_rest(spine1_b) * (_seat_of(tail) as Node3D).transform * tail.transform).origin
	check(tail_rest.distance_to(want_back) < 0.01, "anchors: tail origin = torso back point 5 cm lower (rest %s want %s)" % [tail_rest, want_back])
	check(tail_rest.z < torso.get_center().z, "anchors: tail sits on the back (-Z, the game rig faces +Z) z=%s" % tail_rest.z)
	check(abs(cap.global_position.y - cap_y0 - 0.01) < 0.005, "anchors: hat seat on the stock head = drop 0.05 instead of 0.06 (dy=%s, want +0.01)" % (cap.global_position.y - cap_y0))
	check(cap2.global_position.distance_to(cap.global_position) > 0.03, "anchors: a 2nd copy of the same hat does not z-fight the first (d=%s)" % cap2.global_position.distance_to(cap.global_position))
	check(tail2.global_position.distance_to(tail.global_position) > 0.03, "anchors: a 2nd copy of the same tail is fanned out (d=%s)" % tail2.global_position.distance_to(tail.global_position))

	var g1 := cap.global_position; var t1 := tail.global_position
	for i in 5:
		tl().pose_game_accessory_roots(p)
		await frame()
	check(cap.global_position.distance_to(g1) < 1e-4 and tail.global_position.distance_to(t1) < 1e-4, "anchors: repeated syncs never move the hat/tail")

	sk.set_bone_pose_position(head_b, sk.get_bone_pose_position(head_b) + Vector3(0, 0.3, 0))
	await frame(); await frame()
	check(abs(cap.global_position.y - g1.y - 0.3 * m) < 0.02, "anchors: hat follows the head bone (dy=%s want %s)" % [cap.global_position.y - g1.y, 0.3 * m])
	sk.set_bone_pose_position(head_b, sk.get_bone_pose_position(head_b) - Vector3(0, 0.3, 0))
	await frame(); await frame()

	var g := MeshInstance3D.new(); g.name = "JvmModel0"; g.set_meta("jvm_model", true)
	var bm := BoxMesh.new(); bm.size = Vector3(1.0, stock_top + 0.6, 1.0); g.mesh = bm
	g.position = Vector3(0, (stock_top + 0.6) * 0.5, 0)
	sk.add_child(g)
	await frame(); await frame()
	tl().pose_game_accessory_roots(p)
	await frame()
	check(_anchor_of(cap) == ah and sk.get_node_or_null("JVM_Anchor_head") == ah, "anchors: model swap keeps the hat on the same anchor")
	check(abs(cap.global_position.y - g1.y - 0.6 * m) < 0.03, "anchors: taller model -> hat re-seated on the new head top (dy=%s want %s)" % [cap.global_position.y - g1.y, 0.6 * m])


	var g_cap := cap.global_position
	var arm := MeshInstance3D.new(); arm.name = "JvmArmUpperL"; arm.set_meta("jvm_model", true); arm.mesh = BoxMesh.new()
	sk.add_child(arm); arm.top_level = true; arm.global_position = Vector3(6.0, 1.0, 9.0)
	tl().pose_game_accessory_roots(p)
	await frame()
	check(cap.global_position.distance_to(g_cap) < 1e-3, "anchors: top_level JvmArm* graft parts are ignored (cap moved %s)" % cap.global_position.distance_to(g_cap))
	arm.queue_free()

	rig._acc_roots.erase("head"); cap.queue_free()
	var cap3 := _make_catalog_acc("A101", hm); cap3.transform = sk.get_bone_global_rest(head_b).affine_inverse() * place_cap
	rig._acc_roots["head"] = cap3; rig._acc_ids["head"] = "A101"
	await frame(); await frame()
	check(_anchor_of(cap3) == ah, "anchors: a newly mounted game hat is adopted at once (node_added)")
	tl().pose_game_accessory_roots(p)
	await frame(); await frame()
	var seats := 0
	for c in ah.get_children(): if c.has_meta("jvm_anchor_seat"): seats += 1
	check(seats == 2, "anchors: the swapped-out hat's seat is freed (head seats=%d)" % seats)
	var d: Dictionary = tl().describe_accessory_anchors(p)
	check(d.get("rigs", []).size() >= 1, "anchors: describe_accessory_anchors reports the rig")
	p.queue_free()
	await frame()


func _acc_visible_under(player: Node, id: String) -> bool:
	if player == null: return false
	for n in player.find_children("*", "", true, false):
		var nm := str(n.name)
		if not (nm.begins_with("JvmAcc_" + id) or nm.begins_with("JVM_Worn_" + id) or nm.begins_with("JVM_Mount_Worn_" + id)):
			continue
		if n is Node3D and (n as Node3D).is_visible_in_tree():
			return true
	return false

func _make_catalog_acc(id: String, parent: Node) -> Node3D:
	var root := Node3D.new()
	root.name = "JvmAcc_" + id
	root.set_meta("jvm_acc_id", id)
	var mesh := MeshInstance3D.new()
	mesh.name = "AccMesh"
	mesh.mesh = SphereMesh.new()
	root.add_child(mesh)
	parent.add_child(root)
	return root

func _locker_accessory_visibility() -> void:



	var lk := Control.new()
	lk.name = "LockerAccVis"
	add_child(lk)
	var lrig := mk_rigged("LockerAccVanilla", "locker", lk)
	var player: Node = lrig.get_parent().get_parent()
	var vis: Node = lrig.get_parent()
	var stock := MeshInstance3D.new()
	stock.name = "Body"
	stock.mesh = CapsuleMesh.new()
	vis.add_child(stock)
	stock.visible = false
	var cap := _make_catalog_acc("A100", lrig)
	var tail := _make_catalog_acc("A103", lrig)
	lrig._acc_roots = {"head": cap, "extra": tail}
	lrig._acc_ids = {"head": "A100", "extra": "A103"}
	var ch = GameConfig.character
	var prev_acc = ch.accessories.duplicate(true)
	ch.set_acc_id("head", "A100")
	ch.set_acc_id("extra", "A103")
	tl().apply_worn_locker_accessories_for_test(player)
	await frame()
	await frame()
	check(_acc_visible_under(player, "A100"), "vanilla locker: Champion Cap is visible (catalog or JVM_Worn)")
	check(_acc_visible_under(player, "A103"), "vanilla locker: Flame Tail is visible (catalog or JVM_Worn)")
	check(not stock.visible, "vanilla locker: Visual/Body capsule stays hidden (not forced on)")

	tl().apply_worn_locker_accessories_for_test(player)
	await frame(); await frame()
	check(_acc_visible_under(player, "A100"), "grafted locker: Champion Cap stays visible")
	check(_acc_visible_under(player, "A103"), "grafted locker: Flame Tail stays visible")
	check(not stock.visible, "grafted locker: stock Visual/Body stays hidden")
	ch.accessories = prev_acc
	lk.queue_free()
	await frame()

func _locker_tab() -> void:
	await equip("AE01")
	var lk: Control = load("res://game/character/locker.gd").new(); lk.name = "Locker"; add_child(lk)
	var lrig := mk_rigged("LockerPreview", "locker", lk)
	lk._player = lrig.get_parent().get_parent()
	var vis: Node = lrig.get_parent()
	var stock_body := MeshInstance3D.new()
	stock_body.name = "Body"
	stock_body.mesh = CapsuleMesh.new()
	stock_body.visible = false
	vis.add_child(stock_body)
	lk._outfit_btn = Button.new()
	lk._outfit_btn.text = "showing: captain uniform"
	lk.add_child(lk._outfit_btn)
	await frame(); await frame(); await frame()
	check(lk._rail_btns.has("jvm_full_model"), "locker: \"full model\" tab added")
	var tile: Button = lk._rail_btns.get("jvm_full_model")
	if tile == null: return
	check(tile.text == "full model" and tile.get_index() == lk._rail_btns["uniforms"].get_index() + 1, "tab tile sits right after \"outfits\" in the rail (%d)" % tile.get_index())
	check(lk._rail_btns.keys() == ["face", "body", "voice", "hair", "accessories", "uniforms", "jvm_full_model", "presets", "studio"], "_rail_btns order matches the rail (controller tab cycling) %s" % str(lk._rail_btns.keys()))
	await wait(0.6)
	check(grafts(lrig).size() == 1, "locker preview Player (locker._player) wears the equipped model")
	check(not stock_body.visible, "locker Visual/Body is hidden while the full model is grafted (03:41)")
	check(lk._outfit_btn.text.begins_with("showing:") and "captain" not in lk._outfit_btn.text.to_lower(), "locker banner matches the grafted model, not captain uniform (%s)" % lk._outfit_btn.text)





	stock_body.visible = true
	var body_m: Node3D = lrig._skel.get_node_or_null("bodymesh")
	var head_m: Node3D = lrig._skel.get_node_or_null("headmesh")
	if body_m: body_m.visible = true
	if head_m: head_m.visible = true
	check(not stock_body.visible, "locker Body stays hidden if the game re-shows it after the graft")
	check(body_m == null or not body_m.visible, "locker player_char bodymesh stays hidden if the game re-shows it")
	check(head_m == null or not head_m.visible, "locker headmesh stays hidden if the game re-shows it")
	check(grafts(lrig).size() == 1 and (grafts(lrig)[0] as Node3D).visible, "graft stays visible after the game re-shows stock meshes")
	tile.pressed.emit(); await frame()
	var page: Node = lk._content.get_node_or_null("JvmFullModel")
	check(lk._section == "jvm_full_model" and page != null, "tab tile opens the full model page in the locker's own panel")
	check(tile.get_meta("lit", false) and not lk._rail_btns["face"].get_meta("lit", true), "locker paints our tile lit, the others not")
	lk.cycle_section(1); await frame(); lk.cycle_section(-1); await frame(); await frame()
	check(lk._section == "jvm_full_model" and lk._content.get_node_or_null("JvmFullModel") != null, "controller cycle back onto the tab rebuilds the page")
	page = lk._content.get_node_or_null("JvmFullModel")
	check(_btn(page, "none (vanilla)") != null and _btn(page, "bean glb") != null and _btn(page, "bean two") != null, "page lists none (vanilla) + the model packs")
	check(_btn(page, "in use") != null and _btn(page, "in use").disabled, "equipped model shows as in use")

	_btn(page, "none (vanilla)").pressed.emit(); await frame(); await frame(); await frame()
	check(grafts(lrig).is_empty() and not ml().is_previewing() and _equipped_model() == "" and ml().get_saved_model() == "", "pick none (vanilla): saved immediately (equipped.json has no model)")
	check(graft_nodes_under(lk._player).is_empty(), "preview none: ungraft frees every jvm_model node under the locker player")
	check(not stock_body.visible, "preview none: Visual/Body placeholder stays hidden (ungraft must not force the capsule back on over vanilla)")
	page = lk._content.get_node_or_null("JvmFullModel")
	_btn(page, "bean two").pressed.emit(); await frame(); await frame(); await frame(); await wait(0.6)
	check(grafts(lrig).size() == 1 and ml().get_shown_model() == "AE02" and _equipped_model() == "AE02" and not ml().is_previewing(), "pick bean two: shown on the locker preview and saved immediately")
	check(lk.picks >= 1, "locker's own pick gesture played")
	var st: Dictionary = ml().get_model_settings("AE02")
	check(str(st.get("default_color", "")).begins_with("#") and st.get("color") == st.get("default_color") and not st.get("custom"), "model default colour read from the model (%s)" % st.get("default_color"))
	page = lk._content.get_node_or_null("JvmFullModel")
	check(_btn(page, "save this model") == null and _btn(page, "in use") != null and _btn(page, "in use").disabled, "no separate save step after a pick: shows \"in use\"")

	ml().preview_model("AE01"); await frame(); await frame(); await wait(0.6)
	check(ml().is_previewing() and _equipped_model() == "AE02", "API preview AE01: shown, not saved")
	var tab := ml().get_node_or_null("JvmFullModelTab")
	if tab: tab.call("_rebuild")
	await frame()
	page = lk._content.get_node_or_null("JvmFullModel")
	var use := _btn(page, "save this model")
	check(use != null and not use.disabled, "\"save this model\" offered for an unsaved preview")
	if use: use.pressed.emit()
	await frame(); await frame(); await frame(); await wait(0.6)
	check(_equipped_model() == "AE01" and not ml().is_previewing(), "save this model -> equipped.json model AE01")
	page = lk._content.get_node_or_null("JvmFullModel")
	if page and _btn(page, "bean two"): _btn(page, "bean two").pressed.emit()
	await frame(); await frame(); await frame(); await wait(0.6)
	check(_equipped_model() == "AE02" and ml().get_saved_model() == "AE02", "pick bean two again -> equipped.json model AE02")

	page = lk._content.get_node_or_null("JvmFullModel")
	var sw := _find(page, func(c): return c is Button and c.has_meta("color") and (c.get_meta("color") as Color).to_html(false) == "2f6bff")
	check(sw.size() == 1, "preset swatch row (with the model default first)")
	var sm: Material = gmat(grafts(lrig)[0])
	if sw.size() == 1: sw[0].pressed.emit()
	await frame(); await frame()
	sm = gmat(grafts(lrig)[0])
	check(_alb(sm).is_equal_approx(Color("#2f6bff")), "swatch colour -> shader albedo (%s)" % _alb(sm))
	check(not (sm is ShaderMaterial) or (sm as ShaderMaterial).get_shader_parameter("sss_tint").is_equal_approx(Color("#2f6bff").lerp(Color.WHITE, 0.5)), "tint_light follows the user colour")
	lrig.set_slot_color("jersey", Color(1, 0, 0)); await frame(); await frame()
	check(_alb(sm).is_equal_approx(Color("#2f6bff")), "team colour still ignored with a user colour")
	await wait(0.7)
	check(str(_settings_file().get("AE02", {}).get("color", "")) == "#2F6BFF", "colour saved to model_settings.json (%s)" % str(_settings_file()))

	page = lk._content.get_node_or_null("JvmFullModel")
	var pick := _find(page, func(c): return c is ColorPickerButton)
	if pick.size(): (pick[0] as ColorPickerButton).color_changed.emit(Color("#11aa33"))
	await frame()
	check(_alb(sm).is_equal_approx(Color("#11aa33")), "custom colour picker applies live")
	var reset := _btn(page, "reset to model default")
	check(reset != null and not reset.disabled, "reset to model default offered")
	if reset: reset.pressed.emit()
	await frame(); await frame(); await wait(0.3)
	sm = gmat(grafts(lrig)[0])
	check(_alb(sm).is_equal_approx(Color(0.1, 0.8, 0.9)) and not _settings_file().get("AE02", {}).has("color"), "reset: model's own colour back, colour removed from model_settings.json")

	page = lk._content.get_node_or_null("JvmFullModel")
	var hide := _btn(page, "hide")
	if hide: hide.pressed.emit()
	await frame(); await frame(); await frame(); await wait(0.7)
	var lhl := lrig.get_parent().get_node("HandL") as MeshInstance3D
	check(grafts(lrig).size() == 1 and not lhl.visible and str(_settings_file().get("AE02", {}).get("game_hands", "")) == "hide", "hands: hide -> game hand spheres hidden, saved")
	page = lk._content.get_node_or_null("JvmFullModel")
	var every := _btn(page, "everyone on my screen")
	if every: every.pressed.emit()
	await frame(); await wait(0.7)
	check(grafts(remote_rig).size() == 1, "shown on: everyone -> remote players wear it too")
	page = lk._content.get_node_or_null("JvmFullModel")
	if _btn(page, "just me"): _btn(page, "just me").pressed.emit()
	await frame(); await wait(0.7)
	check(grafts(remote_rig).is_empty(), "shown on: just me -> remote back to the game's model")

	ml().set_model_color("AE01", Color(1, 0, 0)); ml().flush_settings()
	ml().reload_config(); await frame()
	var s1: Dictionary = ml().get_model_settings("AE01")
	check(s1.get("custom") and s1.get("color") == "#FF0000", "per-model colour survives a reload (%s)" % str(s1))

	page = lk._content.get_node_or_null("JvmFullModel")
	if page == null: await frame(); page = lk._content.get_node_or_null("JvmFullModel")
	if page and _btn(page, "bean glb"): _btn(page, "bean glb").pressed.emit()
	await frame(); await frame()
	check(ml().get_shown_model() == "AE01" and not ml().is_previewing() and _equipped_model() == "AE01", "pick bean glb: saved immediately")
	await wait(0.6)
	var gm := gmat(grafts(lrig)[0]) as StandardMaterial3D if grafts(lrig).size() else null
	check(gm != null and gm.albedo_color.is_equal_approx(Color(1, 0, 0)), "glb: user colour -> albedo_color")
	lk.queue_free(); await frame(); await frame(); await frame()
	check(not ml().is_previewing() and ml().get_shown_model() == "AE01" and _equipped_model() == "AE01", "locker closed -> the picked model AE01 is kept (not reverted)")

	var lk3: Control = load("res://game/character/locker.gd").new(); lk3.name = "Locker"; add_child(lk3)
	var lrig3 := mk_rigged("LockerPreview3", "locker", lk3)
	lk3._player = lrig3.get_parent().get_parent()
	await frame(); await frame(); await frame()
	var tile3: Button = lk3._rail_btns.get("jvm_full_model")
	if tile3: tile3.pressed.emit()
	await frame()
	var page3: Node = lk3._content.get_node_or_null("JvmFullModel")
	if page3 and _btn(page3, "none (vanilla)"): _btn(page3, "none (vanilla)").pressed.emit()
	await frame(); await frame()
	lk3.queue_free(); await frame(); await frame(); await frame(); await wait(0.6)
	check(_equipped_model() == "" and ml().get_saved_model() == "" and ml().get_shown_model() == "", "pick vanilla, leave the locker -> stays vanilla (equipped.json, shown)")

	var lk4: Control = load("res://game/character/locker.gd").new(); lk4.name = "Locker"; add_child(lk4)
	await frame(); await frame()
	ml().preview_model("AE02"); await frame(); await frame()
	lk4.queue_free(); await frame(); await frame(); await frame()
	check(_equipped_model() == "AE02" and not ml().is_previewing(), "locker closed with an unsaved preview -> saved (not reverted)")

	var lk2: Control = load("res://game/character/locker.gd").new(); lk2.section_mode = true; add_child(lk2)
	await frame(); await frame()
	check(not lk2._rail_btns.has("jvm_full_model"), "split-screen (section_mode) locker: no tab")
	lk2.queue_free()
	var f := FileAccess.open(cfolder().path_join("model_settings.json"), FileAccess.WRITE); f.store_string("{}"); f.close()
	await equip("AE01")


func _pickers() -> void:
	await wait(0.9)
	check(gate().is_cosmetic_active() and tex_on(), "menu: textures on")
	var st: Dictionary = tl().get_status()
	var balls: Array = tl().list_picker_packs("ball")
	var stages: Array = tl().list_picker_packs("stage")
	var ball_ids := []
	for p in balls: ball_ids.append(String(p.get("id")))
	var stage_ids := []
	for p in stages: stage_ids.append(String(p.get("id")))
	print("  ball packs: ", ball_ids, " stage packs: ", stage_ids)
	check("1007" in ball_ids, "type=ball pack is listed (1007 Galaxy-like)")
	check("1008" in stage_ids, "type=stage pack is listed (1008 Royal-like)")
	check(not ("BA11" in ball_ids), "kind=global texture pack is not a ball tile")
	var ginfo: Dictionary = tl().pack_info("1007")
	check(ginfo.get("wrap") == "equirect" and ginfo.get("type") == "ball" and ginfo.get("label") == "Galaxy",
		"ball pack keeps extra fields (wrap=equirect) and label (%s)" % str(ginfo))
	check(String(st.get("picked_ball", "")) == "" and String(st.get("picked_stage", "")) == "",
		"nothing picked by default (custom packs are not always-on)")

	check(tl().set_picked_ball("1007"), "set_picked_ball 1007")
	check(tl().get_picked_ball() == "1007" and tl().get_status().get("picked_ball") == "1007", "picked ball is 1007")
	var pj = JSON.parse_string(FileAccess.get_file_as_string(OS.get_environment("JV_MODS_DIR").path_join("resources/packs/picker.json")))
	check(pj is Dictionary and String(pj.get("ball")) == "1007", "picker.json persists ball=1007 (%s)" % str(pj))
	check(tl().set_picked_stage("1008"), "set_picked_stage 1008")
	check(tl().get_picked_stage() == "1008", "picked stage is 1008")
	check(not tl().set_picked_ball("BA11"), "set_picked_ball rejects a non-ball pack")
	check(tl().get_picked_ball() == "1007", "rejected set leaves the previous pick")

	check(tl().set_picked_ball(""), "clear ball pick")
	check(tl().get_picked_ball() == "", "custom ball off")

	var root = OS.get_environment("JV_MODS_DIR").path_join("resources/packs/sunrise_ball")
	DirAccess.make_dir_recursive_absolute(root)
	var f = FileAccess.open(root.path_join("skin.json"), FileAccess.WRITE)
	f.store_string('{"id":"1009","name":"Sunrise Ball","type":"ball","preview":"preview.png","ball":{"albedo_tex":"ball_tex.png","wrap":"equirect"}}')
	f.close()

	tl().rescan_picker_packs()
	var after: Array = tl().list_picker_packs("ball")
	var after_ids := []
	for p in after: after_ids.append(String(p.get("id")))
	print("  after drop: ", after_ids)
	check("1009" in after_ids and "1007" in after_ids, "dropped ball pack appears on rescan without a rebuild (%s)" % str(after_ids))
	check(tl().set_picked_ball("1009") and tl().get_picked_ball() == "1009", "new pack is selectable")

	var r2 = OS.get_environment("JV_MODS_DIR").path_join("resources/packs/inferred_ball")
	DirAccess.make_dir_recursive_absolute(r2)
	f = FileAccess.open(r2.path_join("skin.json"), FileAccess.WRITE)
	f.store_string('{"id":"1010","name":"Inferred","kind":"global","ball":{"albedo_tex":"ball_tex.png"}}')
	f.close()
	tl().rescan_picker_packs()
	var inf_ids := []
	for p in tl().list_picker_packs("ball"): inf_ids.append(String(p.get("id")))
	check("1010" in inf_ids, "ball{} block without type is inferred as a ball pack")
	check(tl().get_picked_ball() == "1009", "rescan keeps the current pick")


	var env_ids := []
	for pe in tl().list_picker_packs("stage"): env_ids.append(String(pe.get("id")))
	check("10E1" in env_ids and "10E2" in env_ids, "environment fixture packs are listed (%s)" % str(env_ids))
	check("10E5" in env_ids, "10E5 tutorial-base fixture pack is listed")
	var e5: Dictionary = tl().pack_info("10E5")
	check(str(e5.get("base_stage", "")) == "tutorial", "10E5 pack_info base_stage is tutorial (%s)" % str(e5.get("base_stage", "")))
	var e1: Dictionary = tl().pack_info("10E1")
	var e2: Dictionary = tl().pack_info("10E2")
	check(bool(e1.get("has_environment")) and bool(e1.get("has_sky")), "10E1 pack_info exposes has_environment and has_sky")
	check(bool(e1.get("hide_ground")) and bool(e1.get("hide_env_props")) and bool(e1.get("baked_lighting")), "10E1 hide_ground + hide_env_props + baked_lighting")
	check(bool(e2.get("has_environment")) and not bool(e2.get("hide_ground")), "10E2 has environment and keeps ground")
	check(not e1.has("physics") and not e2.has("physics"), "environment packs do not expose a physics block")

	var probe: Node = get_tree().get_current_scene()
	if probe == null: probe = self
	var we := WorldEnvironment.new()
	we.name = "AnimeSkyEnv"
	var envw := Environment.new()
	envw.background_mode = Environment.BG_COLOR
	we.environment = envw
	probe.add_child(we)

	var main := Node3D.new(); main.name = "Main"; probe.add_child(main)
	main.add_child(_make_court())
	var grass := Node3D.new(); grass.name = "Grass"; main.add_child(grass)
	var hills := Node3D.new(); hills.name = "Hills"; main.add_child(hills)
	var ground := Node3D.new(); ground.name = "EnvFloor"; grass.add_child(ground)

	reset_menu(); _freeplay = true; Lobby.match_kind = "freeplay"
	await wait(0.6)
	check(tl().set_picked_stage("10E1") and tl().get_picked_stage() == "10E1", "pick 10E1 environment pack")
	check(last_swap_stage == "grass" and _freeplay_stage == "grass", "10E1 restages the game kit to base_stage grass (freeplay=%s swap=%s)" % [_freeplay_stage, last_swap_stage])
	tl().apply_picked_cosmetics()
	await wait(0.05)
	check(not hills.visible, "10E1 hide_env_props hides Hills")
	check(not ground.visible, "10E1 hide_ground hides EnvFloor (visual only)")
	var st_sky: Dictionary = tl().get_sky_status()
	var st_env: Dictionary = tl().get_env_status()
	check(bool(st_sky.get("applied")), "sky hook reports applied after 10E1 (%s)" % str(st_sky))
	check(bool(st_env.get("applied")) and int(st_env.get("count", 0)) >= 1, "env hook reports applied after 10E1 (%s)" % str(st_env))
	check(we.environment != envw, "WorldEnvironment was swapped to a new Environment")
	var env_node: Node = main.find_child("JVM_CourtEnv", true, false)
	check(env_node != null, "JVM_CourtEnv is parented under Main, not the scene root")
	check(env_node.get_parent() == main, "JVM_CourtEnv parent is Main")
	check(probe.find_child("JVM_CourtEnv", true, false) == env_node, "only one env instance in the single-scene case")



	var env_meshes: Array = []
	var stack: Array = [env_node]
	while not stack.is_empty():
		var n: Node = stack.pop_back()
		if n is MeshInstance3D: env_meshes.append(n)
		for c in n.get_children(): stack.append(c)
	check(not env_meshes.is_empty(), "10E1 spawned at least one env mesh")
	for mi in env_meshes:
		var mat: Material = mi.material_override
		if mat == null: mat = mi.get_active_material(0)
		check(mat != null, "10E1 env mesh %s has a material" % mi.name)
		if mat is StandardMaterial3D:
			check(mat.transparency == BaseMaterial3D.TRANSPARENCY_DISABLED and mat.render_priority == -128,
				"10E1 env StandardMaterial3D is opaque at render_priority -128 (%s pri=%s tr=%s)" % [mi.name, mat.render_priority, mat.transparency])
		elif mat is ShaderMaterial:
			check(mat.render_priority == -128, "10E1 env ShaderMaterial render_priority -128 (%s pri=%s)" % [mi.name, mat.render_priority])
		var w: AABB = mi.global_transform * mi.get_aabb()
		var top: float = w.position.y + w.size.y
		var lname := String(mi.name).to_lower()
		if "floor" in lname or "ground" in lname or "deck" in lname:
			check(top <= -0.05, "10E1 floor mesh %s top is clamped below y=0 (top=%.3f)" % [mi.name, top])

	check(tl().set_picked_stage("10E2") and tl().get_picked_stage() == "10E2", "pick 10E2 environment pack")
	tl().apply_picked_cosmetics()
	await wait(0.05)
	check(not hills.visible, "10E2 hide_env_props hides Hills")
	check(ground.visible, "10E2 keeps EnvFloor visible")

	check(tl().set_picked_stage("10E5") and tl().get_picked_stage() == "10E5", "pick 10E5 tutorial-base pack")
	check(last_swap_stage == "tutorial" and _freeplay_stage == "tutorial", "10E5 restages the game kit to base_stage tutorial (freeplay=%s swap=%s)" % [_freeplay_stage, last_swap_stage])
	tl().apply_picked_cosmetics()
	await wait(0.05)
	check(not hills.visible, "10E5 hide_env_props hides Hills")
	check(not ground.visible, "10E5 hide_ground hides EnvFloor (visual only); CourtFloor is kept")
	check(bool(tl().get_env_status().get("applied")), "env hook stays applied on 10E2")


	if "10E4" in env_ids:
		check(tl().set_picked_stage("10E4") and tl().get_picked_stage() == "10E4", "pick 10E4 missing-mesh pack")
		tl().apply_picked_cosmetics()
		await wait(0.05)
		check(ground.visible, "10E4 load_glb failure leaves EnvFloor visible")
		check(main.find_child("JVM_CourtEnv", true, false) == null, "10E4 does not spawn JVM_CourtEnv")
		check(not bool(tl().get_env_status().get("applied")), "10E4 env hook reports not applied")

	check(tl().set_picked_stage(""), "clear stage pick after environment")
	tl().apply_picked_cosmetics()
	await wait(0.05)
	check(hills.visible, "clearing the pack restores Hills visibility")
	check(ground.visible, "clearing the pack restores EnvFloor visibility")
	check(not bool(tl().get_sky_status().get("applied")), "sky hook reports not applied after clear")
	check(not bool(tl().get_env_status().get("applied")), "env hook reports not applied after clear")
	check(probe.find_child("JVM_CourtEnv", true, false) == null, "JVM_CourtEnv is removed on clear")




	var menu := Node3D.new(); menu.name = "Menu"; probe.add_child(menu)
	var menu_stage := Node3D.new(); menu_stage.name = "Stage"; menu.add_child(menu_stage)
	menu_stage.add_child(_make_court())
	var menu_grass := Node3D.new(); menu_grass.name = "Grass"; menu_stage.add_child(menu_grass)
	var menu_floor := Node3D.new(); menu_floor.name = "EnvFloor"; menu_grass.add_child(menu_floor)
	var layer := CanvasLayer.new(); layer.name = "CanvasLayer"; menu.add_child(layer)
	var vpc := SubViewportContainer.new(); vpc.name = "SubViewportContainer"; layer.add_child(vpc)
	var vp := SubViewport.new(); vp.name = "SubViewport"; vp.size = Vector2i(128, 128); vpc.add_child(vp)
	var play_main := Node3D.new(); play_main.name = "Main"; vp.add_child(play_main)
	var cam := Camera3D.new(); cam.name = "Camera"; cam.position = Vector3(0, 6.4, 22.5); cam.current = true; play_main.add_child(cam)
	play_main.add_child(_make_court())
	var play_grass := Node3D.new(); play_grass.name = "Grass"; play_main.add_child(play_grass)
	var play_floor := Node3D.new(); play_floor.name = "EnvFloor"; play_grass.add_child(play_floor)

	check(tl().set_picked_stage("10E1") and tl().get_picked_stage() == "10E1", "10E5 pick 10E1 for multi-scene")
	tl().apply_picked_cosmetics()
	await wait(0.05)
	var env_play: Node = play_main.find_child("JVM_CourtEnv", true, false)
	var env_menu: Node = menu_stage.find_child("JVM_CourtEnv", true, false)
	check(env_play != null, "10E5 env is inside SubViewport Main (play/preview camera world)")
	check(env_play != null and env_play.get_parent() == play_main, "10E5 play env parent is SubViewport Main, not get_current_scene()")
	check(env_menu != null, "10E5 menu Court copy also got its own env (revert_env did not drop the other)")
	check(env_menu != env_play, "10E5 two env instances, one per parent")
	check(not play_floor.visible, "10E5 EnvFloor hidden only in the tree where env spawned (play)")
	check(not menu_floor.visible, "10E5 EnvFloor hidden in the menu tree where env also spawned")
	check(env_play.is_visible_in_tree(), "10E5 play env is visible in the tree")
	check(env_play.get_viewport() == cam.get_viewport(), "10E5 play env and play camera share the SubViewport")
	var est: Dictionary = tl().get_env_status()
	check(int(est.get("count", 0)) >= 2, "10E5 env_status count covers both instances (%s)" % str(est))
	var insts: Array = est.get("instances", [])
	check(insts.size() >= 2, "10E5 env_status lists both parents (%s)" % str(est))

	check(tl().set_picked_stage(""), "10E5 clear after multi-scene")
	tl().apply_picked_cosmetics()
	await wait(0.05)
	check(play_floor.visible and menu_floor.visible, "10E5 clearing restores EnvFloor in both trees")
	check(play_main.find_child("JVM_CourtEnv", true, false) == null, "10E5 play env removed on clear")
	check(menu_stage.find_child("JVM_CourtEnv", true, false) == null, "10E5 menu env removed on clear")
	menu.queue_free()
	reset_menu()




	check(tl().set_picked_stage("10E1") and tl().get_picked_stage() == "10E1", "10E6 pick 10E1 before the play court exists")
	tl().apply_picked_cosmetics()
	await wait(0.05)
	var env_before_spawn := int(tl().get_apply_env_count())
	var layer6 := CanvasLayer.new(); layer6.name = "PlayLayer"; add_child(layer6)
	var vpc6 := SubViewportContainer.new(); vpc6.name = "SubViewportContainer"; layer6.add_child(vpc6)
	var vp6 := SubViewport.new(); vp6.name = "SubViewport"; vp6.size = Vector2i(128, 128); vpc6.add_child(vp6)
	var play_main6 := Node3D.new(); play_main6.name = "Main"; vp6.add_child(play_main6)
	var cam6 := Camera3D.new(); cam6.name = "Camera"; cam6.position = Vector3(0, 6.4, 22.5); cam6.current = true; play_main6.add_child(cam6)

	play_main6.add_child(_make_court())
	var play_grass6 := Node3D.new(); play_grass6.name = "Grass"; play_main6.add_child(play_grass6)
	var play_trees6 := Node3D.new(); play_trees6.name = "Trees"; play_grass6.add_child(play_trees6)
	var play_hills6 := Node3D.new(); play_hills6.name = "Hills"; play_grass6.add_child(play_hills6)
	var play_floor6 := Node3D.new(); play_floor6.name = "EnvFloor"; play_grass6.add_child(play_floor6)

	await frame()
	await frame()
	await wait(0.05)
	var env6: Node = play_main6.find_child("JVM_CourtEnv", true, false)
	check(env6 != null, "10E6 env attaches after Court is spawned under the SubViewport")
	check(env6 != null and env6.get_parent() == play_main6, "10E6 env parent is SubViewport Main, not get_current_scene()")
	check(int(tl().get_apply_env_count()) > env_before_spawn, "10E6 apply_env ran on node_added (count %s -> %s)" % [env_before_spawn, tl().get_apply_env_count()])
	check(not play_trees6.visible and not play_hills6.visible, "10E6 hide_env_props hides stock Trees and Hills")
	check(not play_floor6.visible, "10E6 hide_ground hides EnvFloor")
	check(env6.is_visible_in_tree() and env6.get_viewport() == cam6.get_viewport(), "10E6 play env shares the play SubViewport")
	check(tl().set_picked_stage(""), "10E6 clear")
	tl().apply_picked_cosmetics()
	await wait(0.05)
	layer6.queue_free()
	reset_menu()

	await _texture_persist()

func _same_ids(a: Dictionary, b: Dictionary) -> bool:
	for k in a.keys():
		if int(a[k]) != int(b.get(k, 0)):
			return false
	return true

func _make_court() -> Node3D:
	var court := Node3D.new()
	court.name = "Court"
	var floor := MeshInstance3D.new()
	floor.name = "CourtFloor"
	floor.mesh = BoxMesh.new()
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(1, 1, 1)
	floor.material_override = mat
	court.add_child(floor)
	return court


func _assert_cache_mips(tag: String) -> void:
	var st: Dictionary = tl().get_mip_stats()
	print("MIPS ", tag, " ", st)
	check(int(st.get("textures", 0)) > 0, "%s: cache has textures to check" % tag)
	check(int(st.get("without_mips", -1)) == 0, "%s: every cached texture has mipmaps (missing %s)" % [tag, st.get("missing", [])])
	check(int(st.get("with_mips", 0)) == int(st.get("textures", -1)), "%s: with_mips equals texture count" % tag)
	check(int(st.get("s3tc_warnings", 0)) <= 1, "%s: S3TC compress warns at most once (warnings=%s state=%s)" % [tag, st.get("s3tc_warnings"), st.get("s3tc_state")])


func _texture_persist() -> void:
	var loads0 := int(tl().get_disk_load_count())
	var cache0 := int(tl().get_cache_count())
	var ids0: Dictionary = tl().get_cache_ids()
	check(loads0 > 0 and cache0 > 0, "cache populated after first apply (loads=%d cache=%d)" % [loads0, cache0])
	_assert_cache_mips("after first apply")

	var locker := Node3D.new()
	locker.name = "LockerPreview"
	locker.add_child(_make_court())
	add_child(locker)
	check(tl().set_picked_stage("1008"), "persist: pick stage 1008")
	tl().apply_picked_cosmetics()
	await frame()
	await frame()
	check(tl().get_override_count() >= 1, "locker: custom stage applied to preview court")
	check(int(tl().get_disk_load_count()) == loads0, "locker apply did not re-read textures from disk (loads %d -> %d)" % [loads0, tl().get_disk_load_count()])

	locker.queue_free()
	await frame()
	await frame()
	await frame()
	check(int(tl().get_cache_count()) >= cache0, "after locker exit: cache still holds textures (cache=%d was %d)" % [tl().get_cache_count(), cache0])
	check(int(tl().get_disk_load_count()) == loads0, "after locker exit: no disk reload")
	check(_same_ids(ids0, tl().get_cache_ids()), "after locker exit: cached Texture2D instances are the same objects")

	set_lobby("weird", -1)
	await frame()
	await frame()
	check(not tex_on(), "fail-closed: global swaps reverted")
	check(int(tl().get_cache_count()) >= cache0, "revert keeps the process-lifetime cache")
	check(int(tl().get_disk_load_count()) == loads0, "revert does not re-read textures from disk")
	set_lobby("offline", -1)
	await wait(0.9)
	check(tex_on(), "back in menu: global swaps re-applied from cache")
	check(int(tl().get_disk_load_count()) == loads0, "gate re-open did not re-read textures from disk")
	check(_same_ids(ids0, tl().get_cache_ids()), "gate re-open: same cached Texture2D instances")

	_freeplay = true
	var play := Node3D.new()
	play.name = "FreePlay"
	play.add_child(_make_court())
	add_child(play)
	tl().apply_picked_cosmetics()
	await frame()
	await frame()
	check(tl().get_override_count() >= 1, "free play: custom stage re-applied to new court nodes")
	check(int(tl().get_disk_load_count()) == loads0, "free play start did not re-read textures from disk")
	check(_same_ids(ids0, tl().get_cache_ids()), "free play: same cached Texture2D instances, not reloaded")

	check(tl().reload_config(), "reload_config still works while the tier is open")
	await frame()
	check(int(tl().get_disk_load_count()) > loads0, "reload_config is an explicit disk reload (loads %d -> %d)" % [loads0, tl().get_disk_load_count()])
	_assert_cache_mips("after reload_config")
	check(tex_on(), "after explicit reload: global swaps still applied")


	var env_before := int(tl().get_apply_env_count())
	var force_before := int(tl().get_cosmetics_force_count())
	var per_before := int(tl().get_cosmetics_periodic_count())
	var junk := Node.new()
	junk.name = "NodeCountChurn"
	add_child(junk)
	for i in 80:
		junk.add_child(Node.new())
	tl().apply_scene_cosmetics_for_test(false)
	tl().apply_scene_cosmetics_for_test(false)
	check(int(tl().get_apply_env_count()) == env_before, "periodic cosmetics does not call apply_env/load_glb (count %s -> %s)" % [env_before, tl().get_apply_env_count()])
	check(int(tl().get_cosmetics_periodic_count()) == per_before + 2, "periodic cosmetics path ran (%s -> %s)" % [per_before, tl().get_cosmetics_periodic_count()])
	tl().apply_scene_cosmetics_for_test(true)
	check(int(tl().get_cosmetics_force_count()) == force_before + 1, "force/signal cosmetics path still runs")
	junk.queue_free()

	play.queue_free()
	_freeplay = false
	set_lobby("offline", -1)






func _menuperf() -> void:
	reset_menu()
	var mu: Node = gate().get_node_or_null("JVMenuUI")
	var mp: Node = gate().get_node_or_null("JVMenuPerf")
	check(mu != null, "menuperf: menu UI node under the gate")
	check(mp != null, "menuperf: JVMenuPerf is in core (under the gate)")
	check(mp.is_enabled() == true, "menuperf: menu_perf on by default")
	_build_menu_mirror()
	_use_stock_tilt = true
	var fat := Node3D.new()
	fat.name = "MenuFatTree"
	add_child(fat)
	for i in 400:
		var a := Node3D.new()
		a.name = "mfat_%d" % i
		fat.add_child(a)
		var b := MeshInstance3D.new()
		b.name = "mleaf_%d" % i
		a.add_child(b)
	print("MENUPERF tree_nodes=", get_tree().get_node_count())
	_menu_live = true
	_fake_mouse = Vector2(200, 200)
	await wait(0.8)
	check(mu.get_info().get("menu_injected") == true, "menuperf: menu injected")
	mp.reset_counters()

	var t0 := Time.get_ticks_usec()
	var static_frames: Array[float] = []
	while Time.get_ticks_usec() - t0 < 2_000_000:
		var a := Time.get_ticks_usec()
		await frame()
		static_frames.append((Time.get_ticks_usec() - a) / 1000.0)
	var info_static: Dictionary = mp.get_info()
	print("MENUPERF mods_on_static ", _perf_summary(static_frames), " ", info_static)

	mp.reset_counters()
	t0 = Time.get_ticks_usec()
	var move_frames: Array[float] = []
	var step := 0
	while Time.get_ticks_usec() - t0 < 2_000_000:
		var a := Time.get_ticks_usec()
		var tt := (Time.get_ticks_usec() - t0) / 1_000_000.0
		_fake_mouse = Vector2(200.0 + 400.0 * sin(tt * 4.0), 180.0 + 80.0 * cos(tt * 3.0))
		_home.visible = (step / 25) % 2 == 0
		_tilt_cards_like(0.016)
		await frame()
		move_frames.append((Time.get_ticks_usec() - a) / 1000.0)
		step += 1
	var info_move: Dictionary = mp.get_info()
	print("MENUPERF mods_on_move ", _perf_summary(move_frames), " ", info_move)
	_home.visible = true
	check(int(info_move.get("tilt_runs", 0)) > 10, "menuperf: core ran tilt in C++ (%s)" % info_move.get("tilt_runs"))
	check(int(info_move.get("z_skipped", 0)) > 0, "menuperf: skipped no-op z_index writes (%s)" % info_move.get("z_skipped"))

	mp.set_enabled(false)
	mp.reset_counters()
	await wait(0.3)
	var info_off: Dictionary = mp.get_info()
	check(mp.is_enabled() == false, "menuperf: set_enabled(false) sticks")
	check(int(info_off.get("tilt_runs", 0)) == 0, "menuperf: disabled optimizer does not steal tilt (%s)" % info_off)

	mp.set_enabled(false)
	_reset_tilt_transforms()
	_fake_mouse = Vector2(-1, -1)
	_tilt_fake = 0.35
	for i in 80:
		await frame()
	var off_pose: Array = _snapshot_tilt()
	mp.set_enabled(true)
	_reset_tilt_transforms()
	_fake_mouse = Vector2(-1, -1)
	_tilt_fake = 0.35
	for i in 80:
		await frame()
	var on_pose: Array = _snapshot_tilt()
	_tilt_fake = 0.0
	var pose_ok := off_pose.size() == on_pose.size() and off_pose.size() > 0
	var worst := 0.0
	if pose_ok:
		for i in off_pose.size():
			var a: Dictionary = off_pose[i]
			var b: Dictionary = on_pose[i]
			worst = maxf(worst, absf(float(a.rot) - float(b.rot)))
			worst = maxf(worst, (Vector2(a.pos) - Vector2(b.pos)).length())
			worst = maxf(worst, (Vector2(a.scale) - Vector2(b.scale)).length())
	check(pose_ok and worst < 0.08, "menuperf: tilt pose matches stock (n=%d worst=%.5f)" % [on_pose.size(), worst])
	print("MENUPERF pose_match n=", on_pose.size(), " worst=", worst)
	if worst >= 0.08:
		for i in mini(off_pose.size(), on_pose.size()):
			print("MENUPERF pose[", i, "] off=", off_pose[i], " on=", on_pose[i])


	mp.set_enabled(true)
	_menu_live = false
	check(static_frames.size() > 20 and move_frames.size() > 20, "menuperf: recorded frames")

func _perf() -> void:
	var fat := Node3D.new()
	fat.name = "PerfFatTree"
	add_child(fat)
	const N := 2500
	for i in N:
		var a := Node3D.new()
		a.name = "fat_%d" % i
		fat.add_child(a)
		var b := MeshInstance3D.new()
		b.name = "leaf_%d" % i
		a.add_child(b)
	print("PERF fat_nodes=", 1 + N * 2, " tree_nodes=", get_tree().get_node_count())

	var log_path := "user://jv_mods/jv_mods.log"
	var status_path := "user://jv_mods/status.json"
	var startup_frames: Array[float] = []
	var t0 := Time.get_ticks_usec()

	_practice_solo = true
	while Time.get_ticks_usec() - t0 < 3_000_000:
		var a := Time.get_ticks_usec()
		await frame()
		startup_frames.append((Time.get_ticks_usec() - a) / 1000.0)
	print("PERF startup ", _perf_summary(startup_frames), " mode=", gate().get_mode() if gate() else "?")


	var writes := 0
	var last_unix := -1
	var status_logs_before := _perf_count_status_logs(log_path)
	var settled: Array[float] = []
	var cpu0 := _proc_cpu_ms()
	var hits: Array[float] = []
	var last_hit_t := -1.0
	var hit_gaps: Array[float] = []
	t0 = Time.get_ticks_usec()
	while Time.get_ticks_usec() - t0 < 4_000_000:
		var a := Time.get_ticks_usec()
		await frame()
		var dt := (Time.get_ticks_usec() - a) / 1000.0
		settled.append(dt)
		var now_s := (Time.get_ticks_usec() - t0) / 1_000_000.0
		if dt >= 16.67:
			hits.append(dt)
			if last_hit_t >= 0.0:
				hit_gaps.append(now_s - last_hit_t)
			last_hit_t = now_s
		var st := status()
		var u := int(st.get("unix_ms", -1))
		if u != last_unix and last_unix != -1:
			writes += 1
		if u != -1:
			last_unix = u
	var status_logs := _perf_count_status_logs(log_path) - status_logs_before
	if cpu0 >= 0.0:
		print("PERF cpu settled cpu_ms_per_frame=%.3f frames=%d" % [(_proc_cpu_ms() - cpu0) / maxf(1.0, settled.size()), settled.size()])
	print("PERF settled ", _perf_summary(settled), " writes=", writes, " write_hz=", snapped(writes / 4.0, 0.01), " status_log_lines=", status_logs)
	if gate().has_method("get_last_status_build_ms"):
		print("PERF status_build_ms=%.3f cached=%s" % [gate().get_last_status_build_ms(), gate().status_was_cached_build() if gate().has_method("status_was_cached_build") else "?"])

	var dvt: Node = gate().get_node_or_null("JVDevTools") if gate() else null
	if dvt == null:
		dvt = get_node_or_null("/root/JVModGate/JVDevTools")
	var goofy_n: Node = get_node_or_null("/root/JVGoofyMods")
	var feats: Dictionary = goofy_n.get_status().get("features", {}) if goofy_n else {}
	print("PERF goofy_idle_features big_ball=", feats.get("big_ball", "?"), " player_scale=", feats.get("player_scale", "?"), " ball_physics=", feats.get("ball_physics", "?"), " layers=", feats.get("layers", "?"), " max_hits=", feats.get("max_hits_per_team", "?"), " sync=", feats.get("sync", {}))
	if dvt == null or not dvt.has_method("profiler_toggle"):
		print("PERF goofy_idle profiler unavailable dvt=", dvt)
	else:
		var ps: Dictionary = dvt.profiler_toggle(2.0, "goofy-idle-defaults")
		print("PERF goofy_idle profiler_toggle ", ps)
		if str(ps.get("toggled", "")) == "started" or ps.get("ok", false):
			await wait(2.4)
			var st: Dictionary = dvt.profiler_status()
			var jpath := str(st.get("last_json", ""))
			print("PERF goofy_idle capture json=", jpath, " capturing=", st.get("capturing", "?"), " frames=", st.get("frames", "?"))
			if jpath != "" and FileAccess.file_exists(jpath):
				var js = JSON.parse_string(FileAccess.get_file_as_string(jpath))
				var stages: Dictionary = js.get("stages", {}) if js is Dictionary else {}
				for name in ["goofy.process", "goofy.physics", "goofy.net.host_sync_tick", "goofy.net.session_tick"]:
					var s: Dictionary = stages.get(name, {})
					if s.is_empty():
						print("PERF stage ", name, " missing")
					else:
						print("PERF stage %s mean_ms=%.4f p50_ms=%.4f p95_ms=%.4f max_ms=%.4f total_ms=%.3f" % [name, float(s.get("mean_ms", 0)), float(s.get("p50_ms", 0)), float(s.get("p95_ms", 0)), float(s.get("max_ms", 0)), float(s.get("total_ms", 0))])
		else:
			print("PERF goofy_idle profiler_toggle did not start: ", ps)
	if hits.size() > 0:
		var hg := "n/a"
		if hit_gaps.size() > 0:
			hit_gaps.sort()
			hg = "%.3f" % hit_gaps[hit_gaps.size() / 2]
		print("PERF hitches n=", hits.size(), " max=", snapped(hits.max(), 0.01), " median_gap_s=", hg, " times_ms=", hits.slice(0, 12))
	else:
		print("PERF hitches n=0")

	var burst: Array[float] = []
	for k in 8:
		var court := Node3D.new()
		court.name = "Court"
		add_child(court)
		var ball := Node3D.new()
		ball.name = "Ball"
		add_child(ball)
		var a := Time.get_ticks_usec()
		await wait(0.55)
		burst.append((Time.get_ticks_usec() - a) / 1000.0 / 0.55)
		court.queue_free()
		ball.queue_free()
		await frame()

	var churn: Array[float] = []
	var cpu1 := _proc_cpu_ms()
	t0 = Time.get_ticks_usec()
	var spawned: Array[Node] = []
	var step := 0
	while Time.get_ticks_usec() - t0 < 2_000_000:
		var a := Time.get_ticks_usec()
		if step % 8 == 0:
			var n := Node3D.new()
			n.name = "Court" if step % 16 == 0 else "Ball"
			add_child(n)
			spawned.append(n)
		if step % 8 == 4 and spawned.size() > 0:
			var n: Node = spawned.pop_back()
			if is_instance_valid(n):
				n.queue_free()
		await frame()
		churn.append((Time.get_ticks_usec() - a) / 1000.0)
		step += 1
	print("PERF churn ", _perf_summary(churn))
	if cpu1 >= 0.0:
		print("PERF cpu churn cpu_ms_per_frame=%.3f frames=%d" % [(_proc_cpu_ms() - cpu1) / maxf(1.0, churn.size()), churn.size()])
	check(gate() != null and gate().is_active(), "perf: gate active in Free Play")
	check(settled.size() > 30, "perf: recorded settled frames (%d)" % settled.size())


func _proc_cpu_ms() -> float:
	var f := FileAccess.open("/proc/self/stat", FileAccess.READ)
	if f == null:
		return -1.0
	var t := f.get_line()
	var rest := t.substr(t.rfind(")") + 2).split(" ")
	if rest.size() < 13:
		return -1.0
	return (float(rest[11]) + float(rest[12])) * 10.0

func _perf_summary(frames: Array) -> String:
	if frames.is_empty():
		return "n=0"
	var copy: Array = frames.duplicate()
	copy.sort()
	var sum := 0.0
	var mx: float = copy[0]
	var over16 := 0
	var over40 := 0
	for v in copy:
		sum += v
		if v > mx:
			mx = v
		if v >= 16.67:
			over16 += 1
		if v >= 40.0:
			over40 += 1
	var n := copy.size()
	var mean := sum / n
	var p95: float = copy[mini(n - 1, int(round(0.95 * (n - 1))))]
	var p99: float = copy[mini(n - 1, int(round(0.99 * (n - 1))))]
	return "n=%d mean=%.2fms (%.1ffps) p95=%.2f p99=%.2f max=%.2f >16.7=%d >40=%d" % [n, mean, (1000.0 / mean) if mean > 0.0 else 0.0, p95, p99, mx, over16, over40]

func _perf_count_status_logs(path: String) -> int:
	if not FileAccess.file_exists(path):
		return 0
	var t := FileAccess.get_file_as_string(path)
	return t.count("[jv-mods:status] status:")



func _hostile_packs_checks() -> void:
	var lt := log_text()
	var st: Dictionary = tl().get_status()
	check(int(Engine.get_meta("jvm_pwned", 0)) == 0, "hostile packs: the canary script in evil.tres never ran (pwned %d)" % int(Engine.get_meta("jvm_pwned", 0)))
	check(int(st.get("security_refused", 0)) >= 8, "hostile packs: refusals counted in status (%d, last: %s)" % [int(st.get("security_refused", 0)), st.get("security_last_refusal", "")])
	check(lt.contains("refused: unsafe path 'res://_mods/resources/packs/hostile_tres/evil.tres'"), "res:// reference to a pack .tres refused")
	check(lt.contains("refused: unsafe path 'user://evil.tres'"), "user:// reference refused")
	check(lt.contains("'evil.tres' in hostile_tres: .tres is not an allowed pack file type"), ".tres named by a pack refused by type")
	check(lt.contains("hostile_tres/spoof.png: not a valid png"), "spoofed .png (a .tres inside) refused by magic bytes")
	check(lt.contains("hostile_spoof/ball_tex.png: not a valid png"), "spoofed ball texture refused by magic bytes")
	check(lt.contains("refused: unsafe path '../royal_court/court_floor.png'") and lt.contains("refused: unsafe path '../env_ok_test/env.glb'"), "'..' traversal refused (texture + env mesh)")
	check(lt.contains("hostile_bomb/bomb.png: image 20000x20000 exceeds"), "oversize (decompression bomb) image refused before decoding")
	check(lt.contains("is not a game texture"), "global replacement aimed at a pack .tres refused")
	check(lt.contains("environment.lights in hostile_lights: 50 lights, only the first 8 are used"), "environment.lights capped at 8 per pack")
	check(lt.contains("environment.lights[0] in hostile_lights: non-finite value"), "non-finite light values refused")
	check(not bool(tl().pack_info("1E08").get("has_audio", false)), "audio ambience pointing at a pack file is refused (not a native res://game/sfx|ost id)")
	if tl().has_method("audio_apply_pack"):
		check(int(tl().audio_apply_pack("1E08")) == 0 and int(Engine.get_meta("jvm_pwned", 0)) == 0, "hostile audio pack plays nothing and runs nothing")
	var stages := []
	for p in tl().list_picker_packs("stage"): stages.append(String(p.get("id")))
	check("1E01" in stages and "1E05" in stages, "hostile stage packs still listed (data parts only, nothing executed)")


func ld() -> Node: return get_node_or_null("/root/JVLoader")
func bridge() -> Dictionary: return ld().get_bridge_status() if ld() else {}
func _loader() -> void:
	var kind := OS.get_environment("JV_GOOFY")
	var live := kind == "api" or kind == "real"
	var gdir := OS.get_environment("JV_MODS_DIR").path_join("goofy")
	var gfile_before := FileAccess.get_file_as_string(gdir.path_join("test.json"))
	var stub: Node = null
	if kind == "api":
		stub = load("res://fake_goofy_api.gd").new(); stub.name = "JVGoofyMods"
		get_tree().root.add_child(stub)
	await wait(0.9)
	var gfiles_before := DirAccess.get_files_at(gdir)
	check(ld() != null, "one loader node /root/JVLoader")
	_hostile_packs_checks()
	check(tl() != null and ml() != null, "texture + model subsystems at their old paths (/root/JVTextureLoader, /root/JVModelLoader)")
	check(gate().is_cosmetic_active() and tex_on(), "textures: global pack swap applied through the unified loader")
	var ms: Dictionary = ml().get_status()
	check(ms.get("state") == "active", "models subsystem active (%s)" % str(ms.get("reason")))
	var stages := []
	for p in tl().list_picker_packs("stage"): stages.append(String(p.get("id")))
	check("1008" in stages and "1022" in stages and "1023" in stages, "stage packs load (%s)" % str(stages))
	check(ld().is_loader_map("1022") and not ld().is_loader_map("BA11") and not ld().is_loader_map(""), "is_loader_map: stage packs only")
	check(ld().get_map_goofy_block("1022").has("ball.gravity") and ld().get_map_goofy_block("1008").is_empty(), "goofy section read from skin.json (1022 has one, 1008 none)")
	var libs: Dictionary = status().get("libraries", {})
	check(libs.get("loader", {}).get("state") == "active" and libs.get("textures", {}).get("state") == "active" and libs.has("models"), "status.json: loader + textures + models (%s)" % str(libs.keys()))
	var phys: Resource = load("res://game/ball/ball_physics.tres")
	var g0: float = phys.gravity
	check(is_equal_approx(g0, 11.0), "stock ball gravity 11 before any map")
	check(tl().set_picked_stage("1022"), "pick stage 1022 (goofy section: ball.gravity mul 0.4)")
	await wait(0.4)
	check(is_equal_approx(phys.gravity, g0) and ld().get_active_map() == "", "picked but no court in the scene -> nothing applied (%s)" % bridge().get("why"))
	var court := Node3D.new(); court.name = "Court"; add_child(court)
	await wait(0.5)
	check(ld().get_active_map() == "1022", "court appears -> loader map 1022 in play")
	check(tl().get_status().get("picked_stage") == "1022", "stage pack still applied by the texture subsystem")
	if live:
		var gf: Node = get_node("/root/JVGoofyMods")
		check(bridge().get("goofy_found") and String(bridge().get("goofy_api_version")).begins_with("1."), "bridge found Goofy API %s" % bridge().get("goofy_api_version"))
		check(is_equal_approx(phys.gravity, g0 * 0.4), "goofy ON: map's goofy section applied (gravity %.2f, want %.2f)" % [phys.gravity, g0 * 0.4])
		check(bridge().get("state") == "applied" and bridge().get("applied_pack") == "1022" and gf.get_loader_map() == "1022", "bridge: apply_pack_params(1022) + set_loader_map(1022) (%s)" % str(bridge()))
		if stub: check(stub.calls.size() == 3 and stub.calls[0] == ["set_loader_map", ""] and stub.calls[1] == ["set_loader_map", "1022"] and stub.calls[2][0] == "apply_pack_params" and stub.calls[2][3] == 1 and not stub.calls[2][2].has("version"),
			"calls: set_loader_map('') on the stock menu, then set_loader_map(1022) + apply_pack_params(id, params without 'version', version 1) (%s)" % str(stub.calls))
		var n_apply: int = bridge().get("applies")
		await wait(0.6)
		check(int(bridge().get("applies")) == n_apply, "no re-apply every tick")

		set_lobby("custom", 2); await wait(0.6)
		check(not gate().is_active() and gate().is_cosmetic_active(), "public lobby: goofy tier closed, textures open")
		check(is_equal_approx(phys.gravity, g0), "goofy disabled: goofy section has no effect, stock gravity (%.2f)" % phys.gravity)
		check(ld().get_active_map() == "1022" and not bridge().get("goofy_enabled"), "goofy disabled: map still loads normally")
		set_lobby("offline", -1); await wait(0.9)
		check(is_equal_approx(phys.gravity, g0 * 0.4), "goofy enabled again on the same map -> live again")
		tl().set_picked_stage("1008"); await wait(0.5)
		check(is_equal_approx(phys.gravity, g0) and bridge().get("applied_pack") == "" and gf.get_loader_map() == "1008", "map without goofy section: block cleared, loader map 1008, stock gravity")
		tl().set_picked_stage("1023"); await wait(0.5)
		var lr: Dictionary = bridge().get("last_result", {})
		check(ld().get_active_map() == "1023" and "not.a.param" in lr.get("ignored", []) and is_equal_approx(phys.gravity, g0 * 0.5),
			"unknown goofy keys ignored by goofy, known ones applied, map in play (%s)" % str(lr))
		tl().set_picked_stage("1022"); await wait(0.5)
		check(is_equal_approx(phys.gravity, g0 * 0.4), "back on 1022 -> applied again")
	else:
		check(is_equal_approx(phys.gravity, g0), "goofy %s: goofy section ignored, stock gravity" % ("NOT installed" if kind == "none" else "without API (old jvm_goofy)"))
		check(bridge().get("state") == "idle" and String(bridge().get("why")).find("ignored") != -1 and int(bridge().get("applies")) == 0, "bridge ignores silently (%s)" % bridge().get("why"))
		if kind == "old": check(goofy() != null and is_equal_approx(Engine.time_scale, 0.5), "old goofy keeps running its own config")
	court.queue_free(); await wait(0.5)
	check(ld().get_active_map() == "" and is_equal_approx(phys.gravity, g0), "leave map -> goofy section reverted, stock gravity")
	if live:
		check(get_node("/root/JVGoofyMods").get_loader_map() == "", "leave map -> set_loader_map('')")
		if stub: check(stub.layers.is_empty(), "no pack layers left in goofy")
	tl().set_picked_stage("")
	check(FileAccess.get_file_as_string(gdir.path_join("test.json")) == gfile_before and DirAccess.get_files_at(gdir) == gfiles_before, "user goofy config files never written by the loader")
	finish()
