extends RefCounted




var d: Node
const M := "gstest"
var changes: Array = []
var reads := 0

func api() -> Object: return Engine.get_singleton("JVModAPI") if Engine.has_singleton("JVModAPI") else null
func _on_change(id: String, value: Variant, source: String) -> void: changes.append([id, value, source])
func _on_read(_id: String, value: Variant, _source: String) -> Variant:
	reads += 1
	return float(value) * 2.0
func _migrate(from: int, values: Dictionary) -> Dictionary:
	var out := values.duplicate()
	if from == 1 and out.has("gstest.speed"):
		out["gstest.pace"] = out["gstest.speed"]; out.erase("gstest.speed")
	out["gstest._from"] = from
	return out

func defs(version: int) -> Array:
	var speed_id := "gstest.speed" if version == 1 else "gstest.pace"
	return [
		{"id": speed_id, "type": "float", "default": 1.0, "min": 0.5, "max": 2.0, "step": 0.1, "label": "speed", "tooltip": "x speed", "category": "test", "sync": "gameplay"},
		{"id": "gstest.lives", "type": "int", "default": 3, "min": 1, "max": 9, "label": "lives", "category": "test", "sync": "gameplay"},
		{"id": "gstest.wall", "type": "bool", "default": false, "label": "walls", "category": "test", "sync": "gameplay"},
		{"id": "gstest.mode", "type": "enum", "options": ["calm", "wild"], "default": "calm", "label": "mode", "category": "test", "sync": "gameplay"},
		{"id": "gstest.wind", "type": "vec", "default": Vector3.ZERO, "min": -5.0, "max": 5.0, "label": "wind", "category": "test", "sync": "gameplay"},
		{"id": "gstest.boost", "type": "float", "default": 1.0, "min": 0.0, "max": 3.0, "label": "boost", "category": "test", "sync": "gameplay", "on_read": Callable(self, "_on_read")},
	]

func lan(host: bool) -> void:
	d.reset_menu(); d.set_lobby("lan", -1); Net.active = true; Net.party_lan = true; Net.is_host = host
	await d.wait(1.0)
func offline() -> void:
	d.reset_menu(); d.set_lobby("offline", -1); Net.is_host = false
	await d.wait(1.0)
func public_lobby() -> void:
	d.reset_menu(); d.set_lobby("custom", 2); Net.active = true; Net.custom_lobby = true; Net.privacy = 0; Net.is_host = false
	await d.wait(1.0)

func run(driver: Node) -> void:
	d = driver
	await d.wait(1.2)
	var a := api()
	d.check(a != null and a.get_api_version() == "1.3.0", "CORE API 1.3.0")
	if a == null: return
	var cap_ids: Array = a.get_capabilities().map(func(c): return c["id"])
	d.check("gameplay.settings" in cap_ids and "gameplay.settings_sync" in cap_ids and "game.rules" in cap_ids, "capabilities gameplay.settings / gameplay.settings_sync / game.rules present")
	d.check(a.get_capabilities().filter(func(c): return c["id"] in ["gameplay.settings", "gameplay.settings_sync", "game.rules"] and c["status"] == "impl").size() == 3, "all three at status impl")
	d.check(a.gameplay_register(M, defs(1), {}).get("ok", true) == false, "unregistered mod: refused")
	a.register_mod(M, "1.3", PackedStringArray(["gameplay.settings", "gameplay.settings_sync", "gate.mode", "config.state"]))
	var ex: Array = a.gameplay_list(M).filter(func(x): return String(x["id"]).begins_with("low_grav."))
	d.check(ex.size() == 2 and a.gameplay_set("low_grav", "low_grav.gravity", 0.5)["ok"], "docs example mod (mods/api_mods/low_grav) registered its 2 settings")
	await d.frame()
	var exn = d.get_node_or_null("/root/JVScriptMods/low_grav")
	d.check(exn != null and is_equal_approx(exn.gravity_mul, 0.5), "docs example: on_change delivered the value")
	await _registry(a)
	await _settings_tab(a)
	await _goofy_migration(a)
	await _rules(a)
	await _missing_mods(a)


func _registry(a: Object) -> void:
	var bad: Array = [
		{"id": "gstest.trail", "type": "float", "default": 1.0, "sync": "visual"},
		{"id": "gstest.nosync", "type": "float", "default": 1.0},
		{"id": "other.thing", "type": "float", "default": 1.0, "sync": "gameplay"},
		{"id": "gstest.oob", "type": "float", "default": 5.0, "min": 0.0, "max": 1.0, "sync": "gameplay"},
		{"id": "gstest.kind", "type": "colour", "default": 1.0, "sync": "gameplay"},
	]
	var r: Dictionary = a.gameplay_register(M, bad, {})
	d.check(not r["ok"] and (r["registered"] as Array).is_empty() and (r["errors"] as Array).size() == 5, "bad defs refused: visual-only, no sync class, foreign id, default out of range, unknown type (%s)" % str(r["errors"]).left(160))
	d.check(str(r["errors"]).contains("cosmetics stay per-player"), "visual-only refusal says why")
	r = a.gameplay_register(M, defs(1), {"version": 1, "on_change": Callable(self, "_on_change")})
	d.check(r["ok"] and (r["registered"] as Array).size() == 6 and r["migrated_from"] == 0, "6 settings registered (bool, int, float, enum, vec, hooked float)")
	var gr: Dictionary = a.gameplay_register("goofy", [{"id": "gstest.speed", "type": "float", "default": 1.0, "sync": "gameplay"}], {})
	d.check(not gr["ok"], "another mod cannot register into gstest.*")
	d.check(a.gameplay_role(M) == "local", "menu: role local (%s)" % a.gameplay_role(M))
	d.check(is_equal_approx(a.gameplay_get(M, "gstest.speed"), 1.0) and a.gameplay_get(M, "gstest.mode") == "calm" and a.gameplay_get(M, "gstest.wind") == Vector3.ZERO and a.gameplay_get(M, "gstest.lives") == 3, "defaults read back through the hook")
	var s: Dictionary = a.gameplay_set(M, "gstest.speed", 1.73)
	d.check(s["ok"] and is_equal_approx(a.gameplay_get(M, "gstest.speed"), 1.7), "set snaps to step (1.73 -> 1.7)")
	a.gameplay_set(M, "gstest.speed", 9.0)
	d.check(is_equal_approx(a.gameplay_get(M, "gstest.speed"), 2.0), "set clamps to max")
	d.check(not a.gameplay_set(M, "gstest.speed", "fast")["ok"] and not a.gameplay_set(M, "gstest.mode", "nope")["ok"] and not a.gameplay_set(M, "gstest.nope", 1)["ok"], "wrong type / unknown option / unknown id refused")
	d.check(a.gameplay_set(M, "gstest.mode", "wild")["ok"] and a.gameplay_get(M, "gstest.mode") == "wild", "enum by option name")
	d.check(a.gameplay_set(M, "gstest.wind", [1, -9, 2])["ok"] and a.gameplay_get(M, "gstest.wind") == Vector3(1, -5, 2), "vec from an array, clamped per component")
	a.gameplay_set(M, "gstest.wall", true); a.gameplay_set(M, "gstest.lives", 7.6)
	d.check(a.gameplay_get(M, "gstest.wall") == true and a.gameplay_get(M, "gstest.lives") == 8, "bool + int (rounded)")
	await d.frame()
	d.check(changes.any(func(c): return c[0] == "gstest.speed" and is_equal_approx(c[1], 2.0) and c[2] == "local"), "on_change fired with (id, value, source) (%d calls)" % changes.size())
	a.gameplay_set(M, "gstest.boost", 1.25)
	var rb := reads
	d.check(is_equal_approx(a.gameplay_get(M, "gstest.boost"), 2.5) and reads > rb, "on_read translates the value (1.25 -> 2.5)")
	a.gameplay_set(M, "gstest.boost", 2.0)
	d.check(is_equal_approx(a.gameplay_get(M, "gstest.boost"), 3.0), "on_read result clamped to the range (4.0 -> 3.0)")
	d.check(not a.gameplay_set("goofy", "gstest.speed", 1.0)["ok"], "a mod cannot set another mod's setting")
	var info: Dictionary = a.gameplay_info(M, "gstest.speed")
	d.check(info["label"] == "speed" and info["category"] == "test" and info["type"] == "float" and info["source"] == "local" and not info["locked"], "gameplay_info")
	var p := OS.get_environment("JV_MODS_DIR").path_join("api/state/gameplay/gstest.json")
	var saved = JSON.parse_string(FileAccess.get_file_as_string(p))
	d.check(saved is Dictionary and int(saved["version"]) == 1 and is_equal_approx(saved["values"]["gstest.speed"], 2.0) and saved["values"]["gstest.mode"] == "wild", "persisted per mod: mods/api/state/gameplay/gstest.json")
	d.check(not saved["values"].has("gstest.lives") or saved["values"]["gstest.lives"] == 8, "defaults are not stored")

	d.check(a.gameplay_unregister(M) == 6 and a.gameplay_get(M, "gstest.speed") == null, "unregister")
	r = a.gameplay_register(M, defs(2), {"version": 2, "migrate": Callable(self, "_migrate")})
	d.check(r["ok"] and r["migrated_from"] == 1 and is_equal_approx(a.gameplay_get(M, "gstest.pace"), 2.0) and a.gameplay_get(M, "gstest.mode") == "wild", "schema 1 -> 2 migration: speed moved to pace, other values kept")
	saved = JSON.parse_string(FileAccess.get_file_as_string(p))
	d.check(int(saved["version"]) == 2 and not saved["values"].has("gstest.speed") and not saved["values"].has("gstest._from"), "migrated state saved as schema 2 (unknown ids dropped)")

	await public_lobby()
	rb = reads
	d.check(a.gameplay_role(M) == "off" and is_equal_approx(a.gameplay_get(M, "gstest.pace"), 1.0) and is_equal_approx(a.gameplay_get(M, "gstest.boost"), 1.0) and reads == rb, "public lobby: every read = stock, on_read not consulted")
	d.check(a.gameplay_info(M, "gstest.pace")["local"] == 2.0, "public lobby: the player's own value is kept, just not used")
	d.check(not a.gameplay_snapshot(M)["ok"] and a.gameplay_snapshot_bytes(M).is_empty(), "public lobby: no snapshot (sync closed)")

	await lan(true)
	d.check(a.gameplay_role(M) == "local" and is_equal_approx(a.gameplay_get(M, "gstest.pace"), 2.0), "LAN host: own values")
	var snap: Dictionary = a.gameplay_snapshot(M)
	d.check(snap["ok"] and is_equal_approx(snap["values"]["gstest.pace"], 2.0) and snap["values"]["gstest.mode"] == "wild" and String(snap["hash"]).length() == 16, "host snapshot: effective values + hash")
	var bytes: PackedByteArray = a.gameplay_snapshot_bytes(M)
	d.check(bytes.get_string_from_utf8().begins_with("JVGS 1\n") and bytes.get_string_from_utf8().contains("gstest.pace=2"), "host snapshot bytes (JVGS 1)")
	d.check(not a.gameplay_apply_snapshot(M, bytes)["ok"], "host refuses to apply a snapshot (its own values are authoritative)")

	await lan(false)
	d.check(a.gameplay_role(M) == "client" and is_equal_approx(a.gameplay_get(M, "gstest.pace"), 1.0) and a.gameplay_get(M, "gstest.mode") == "calm", "LAN client, nothing from the host yet: stock (never the local value)")
	d.check(not a.gameplay_set(M, "gstest.pace", 1.5)["ok"] and a.gameplay_info(M, "gstest.pace")["locked"], "LAN client: settings locked")
	var hb := bytes.get_string_from_utf8().replace("gstest.pace=2", "gstest.pace=99") + "missing.mod=1\n"
	var ap: Dictionary = a.gameplay_apply_snapshot(M, hb.to_utf8_buffer())
	d.check(ap["ok"] and ap["unknown"] == 1 and is_equal_approx(a.gameplay_get(M, "gstest.pace"), 2.0) and a.gameplay_get(M, "gstest.mode") == "wild" and a.gameplay_info(M, "gstest.pace")["source"] == "host", "client applies the host's bytes: values clamped to the client's range, unknown ids ignored")
	d.check(not a.gameplay_apply_snapshot(M, "JVGS 9\ngstest.pace=1\n".to_utf8_buffer())["ok"] and not a.gameplay_apply_snapshot(M, "rm -rf".to_utf8_buffer())["ok"] and is_equal_approx(a.gameplay_get(M, "gstest.pace"), 2.0), "bad payloads rejected, values unchanged")
	ap = a.gameplay_apply_snapshot(M, {"version": 1, "values": {"gstest.lives": 5, "gstest.wall": "yes"}})
	d.check(ap["ok"] and ap["applied"] == 1 and ap["rejected"] == 1 and a.gameplay_get(M, "gstest.lives") == 5 and is_equal_approx(a.gameplay_get(M, "gstest.pace"), 1.0), "dictionary snapshot: complete replacement, bad value rejected")
	d.check(a.gameplay_clear_host(M) and a.gameplay_get(M, "gstest.lives") == 3, "clear_host -> stock")
	await offline()
	d.check(a.gameplay_role(M) == "local" and is_equal_approx(a.gameplay_get(M, "gstest.pace"), 2.0) and a.gameplay_get(M, "gstest.lives") == 8, "offline again: own values back")


func tab_of(panel: Node) -> Node: return panel._tabs.get_node_or_null("mods") if panel._tabs else null
func rows_for(panel: Node, id: String) -> Array:
	var t := tab_of(panel)
	if t == null: return []
	var col: Node = t.get_child(0).get_child(0)
	return col.get_children().filter(func(c): return c.get_meta("jvm_gs_id", "") == id)
func find_cls(n: Node, cls: String) -> Node:
	if n.is_class(cls): return n
	for c in n.get_children():
		var f := find_cls(c, cls)
		if f: return f
	return null

func _settings_tab(a: Object) -> void:
	var panel = load("res://fake_settings_panel.gd").new()
	panel.name = "SettingsPanel"
	d.add_child(panel)
	await d.wait(0.6)
	var t := tab_of(panel)
	d.check(t != null and t.get_meta("jvm_gs", false), "Settings panel: 'mods' tab added")
	d.check(panel._tabs.get_tab_count() == 3 and panel._tabs.current_tab == 0, "added after the stock tabs; the open tab is unchanged")
	if t == null: return
	var col: Node = t.get_child(0).get_child(0)
	var subs: Array = col.get_children().filter(func(c): return c.get_node_or_null("Sub") != null).map(func(c): return c.get_node("Sub").text)
	d.check("gameplay" in subs and "test" in subs and "rules" in subs and "ball" in subs, "subheaders: gameplay + one per category (%s)" % str(subs))
	d.check(rows_for(panel, "gstest.wind").size() == 3 and rows_for(panel, "gstest.pace").size() == 1 and rows_for(panel, "gstest.mode").size() == 1, "one row per setting (vec: one per component), built with the panel's own builders")
	var cb: CheckBox = find_cls(rows_for(panel, "gstest.wall")[0], "CheckBox")
	d.check(cb != null and cb.button_pressed and cb.tooltip_text == "", "bool row shows the value")
	cb.button_pressed = false
	await d.frame()
	d.check(a.gameplay_get(M, "gstest.wall") == false, "toggling the row writes the registry")
	var sl: HSlider = find_cls(rows_for(panel, "gstest.pace")[0], "HSlider")
	d.check(sl != null and is_equal_approx(sl.value, 2.0) and sl.tooltip_text == "x speed", "slider row: value + tooltip")
	sl.value = 0.7
	await d.frame()
	d.check(is_equal_approx(a.gameplay_get(M, "gstest.pace"), 0.7), "dragging the slider writes the registry")

	await lan(false)
	a.gameplay_apply_snapshot(M, {"values": {"gstest.pace": 1.9}})
	await d.wait(0.5)
	sl = find_cls(rows_for(panel, "gstest.pace")[0], "HSlider")
	d.check(sl != null and not sl.editable and is_equal_approx(sl.value, 1.9) and rows_for(panel, "gstest.pace")[0].modulate.a < 0.9, "LAN client: rows greyed out, showing the host's value (%s)" % (sl.value if sl else -1))
	cb = find_cls(rows_for(panel, "gstest.wall")[0], "CheckBox")
	d.check(cb.disabled, "LAN client: checks disabled")
	d.check(col.get_node("JVGameplayNote").text.contains("host"), "note says the host controls them")
	await offline()
	await d.wait(0.4)
	sl = find_cls(rows_for(panel, "gstest.pace")[0], "HSlider")
	d.check(sl.editable and is_equal_approx(sl.value, 0.7), "offline: editable again, own value")
	panel.rebuild()
	await d.wait(0.5)
	d.check(tab_of(panel) != null and rows_for(panel, "gstest.pace").size() == 1, "after the game rebuilds the panel the tab comes back")
	panel.queue_free()


func _goofy_migration(a: Object) -> void:
	var g: Node = d.goofy()
	var ph = load("res://game/ball/ball_physics.tres")
	var ids: Array = a.gameplay_list(M).filter(func(s): return String(s["id"]).begins_with("goofy.")).map(func(s): return s["id"])
	d.check(ids.size() == 12 and "goofy.player_scale" in ids and "goofy.topspin_spike_boost" in ids, "goofy registered 12 settings including player_scale + topspin_spike_boost (%s)" % str(ids))
	var gs_saved = JSON.parse_string(FileAccess.get_file_as_string(OS.get_environment("JV_MODS_DIR").path_join("api/state/gameplay/goofy.json")))
	d.check(gs_saved is Dictionary and int(gs_saved["version"]) == 2, "goofy settings state saved as schema 2")
	d.check(g.get_rules()["registry"], "goofy reads its ported settings through the registry")
	var gv := func(id): return a.gameplay_get(M, id)
	d.check(is_equal_approx(gv.call("goofy.time_scale"), 0.5) and is_equal_approx(gv.call("goofy.ball_size"), 1.5) and is_equal_approx(gv.call("goofy.gravity_scale"), 0.8) and is_equal_approx(gv.call("goofy.bounce_scale"), 1.2), "legacy keys imported: time_scale, big_ball, gravity_scale, bounce_scale")
	d.check(is_equal_approx(gv.call("goofy.air_drag"), 0.3) and is_equal_approx(gv.call("goofy.curve_scale"), 2.0), "ball_params imported: flight.air_drag 0.3, flight.curve_scale {mul 2} -> 2")
	d.check(gv.call("goofy.rules.max_team_touches") == 4 and gv.call("goofy.rules.double_hit_foul") == false, "rules imported: max_hits_per_team 4, no_double_hit true -> foul off")
	var cfgp := OS.get_environment("JV_MODS_DIR").path_join("goofy/test.json")
	d.check(FileAccess.get_file_as_string(cfgp).contains("\"big_ball\":1.5"), "the old config file is not changed")
	d.check(is_equal_approx(Engine.time_scale, 0.5) and is_equal_approx(ph.gravity, 11.0 * 0.8) and is_equal_approx(ph.floor_bounce, 0.5 * 1.2) and is_equal_approx(float(g.get_param("flight.air_drag")), 0.3) and is_equal_approx(float(g.get_param("flight.curve_scale")), 2.0), "applied from the registry: time scale, gravity, bounce, drag, curve (g %.2f)" % ph.gravity)
	a.gameplay_set(M, "goofy.gravity_scale", 1.0)
	d.check(is_equal_approx(gv.call("goofy.gravity_scale"), 0.8), "another mod cannot edit goofy settings")
	g.call("_on_panel_changed", "jvm_max_hits", 0)
	await d.frame()
	d.check(gv.call("goofy.rules.max_team_touches") == 3, "Modifiers panel row edits the registry")

	FileAccess.open(cfgp, FileAccess.WRITE).store_string('{"time_scale":0.75,"big_ball":1.5,"ball_physics":{"gravity_scale":0.8,"bounce_scale":1.2},"ball_params":{"flight.air_drag":0.3,"flight.curve_scale":{"mul":2}},"max_hits_per_team":0,"no_double_hit":true}')
	g.reload_config()
	await d.wait(0.3)
	d.check(is_equal_approx(gv.call("goofy.time_scale"), 0.75) and is_equal_approx(Engine.time_scale, 0.75), "editing time_scale in the old JSON is adopted (0.75)")

	await public_lobby()
	d.check(is_equal_approx(Engine.time_scale, 1.0) and is_equal_approx(ph.gravity, 11.0) and is_equal_approx(gv.call("goofy.gravity_scale"), 1.0), "public lobby: time scale + ball physics stock")
	await offline()
	d.check(is_equal_approx(Engine.time_scale, 0.75) and is_equal_approx(ph.gravity, 11.0 * 0.8), "offline again: registry values re-applied")


func _rules(a: Object) -> void:
	var g: Node = d.goofy()
	var feel = load("res://game/player/player_feel.tres")
	d.gball = Ball.new(); d.gball.name = "RulesBall"; d.add_child(d.gball)
	var me: Player = d.mk_player("RMe", 1); var mate: Player = d.mk_player("RMate", 1); var foe: Player = d.mk_player("RFoe", -1); var foe2: Player = d.mk_player("RFoe2", -1)
	d._rally = Rally.new(); d._rally.players = [me, mate, foe, foe2]
	await d.wait(1.2)
	var info: Dictionary = a.game_rules_info("goofy")
	d.check(info.get("touches_per_side") == 3 and info.get("double_hit") == "bar" and info.get("block_counts") == false and info.get("over_team_limit") == "fault", "game_rules_info: stock rules as the game has them (%s)" % str(info).left(160))

	for id in ["goofy.rules.double_hit_foul", "goofy.rules.max_consecutive_touches", "goofy.rules.max_team_touches", "goofy.rules.block_classic"]: _reset(a, id)
	await d.frame()
	var st: Dictionary = g.get_rules()
	d.check(st["stock"] and st["double_hit_foul"] and st["max_consecutive_touches"] == 1 and st["max_team_touches"] == 3 and not feel.block_classic, "defaults = stock rules")
	await d.new_rally()
	await d.touch(me); var dbl: bool = await d.touch(me)
	d.check(not dbl, "stock: a second touch in a row is barred")
	await d.new_rally()
	await d.touch(me); await d.touch(mate); await d.touch(me); await d.touch(mate)
	d.check(d._rally.fault == "four touches", "stock: 4th team touch = 'four touches' fault")
	await d.new_rally()
	await d.touch(foe, Rally.KIND_BLOCK); var after_block: bool = await d.touch(foe)
	d.check(after_block and d._rally.touch_count == 1, "stock: a block does not count (blocker may play next, count 1)")

	_g(a, "goofy.rules.double_hit_foul", false)
	await d.new_rally()
	var ok2: bool = await d.touch(me); var ok3: bool = await d.touch(me); var ok4: bool = await d.touch(me)
	d.check(ok2 and ok3 and ok4 and d._rally.fault == "", "foul off: three touches in a row by one player allowed")
	await d.touch(me)
	d.check(d._rally.fault == "four touches", "foul off: still a fault on the 4th team touch")

	_g(a, "goofy.rules.double_hit_foul", true); _g(a, "goofy.rules.max_consecutive_touches", 2)
	await d.new_rally()
	var c1: bool = await d.touch(me); var c2: bool = await d.touch(me); var c3: bool = await d.touch(me)
	d.check(c1 and c2 and not c3, "max 2 in a row: 3rd consecutive touch barred")
	var m1: bool = await d.touch(mate)
	d.check(m1 and d._rally.fault == "", "teammate may play after the streak")
	await d.new_rally()
	await d.touch(foe, Rally.KIND_BLOCK); var b1: bool = await d.touch(foe); var b2: bool = await d.touch(foe); var b3: bool = await d.touch(foe)
	d.check(b1 and b2 and not b3, "a block is not part of the streak: block + 2 touches, the 3rd barred")

	_g(a, "goofy.rules.max_consecutive_touches", 1)

	_g(a, "goofy.rules.max_team_touches", 2)
	await d.new_rally()
	await d.touch(me); await d.touch(mate)
	d.check(d._rally.fault == "", "team touches 2: two touches fine")
	await d.touch(me)
	d.check(d._rally.fault == "four touches", "team touches 2: the 3rd is a fault (point to the other team)")
	_g(a, "goofy.rules.max_team_touches", 5)
	await d.new_rally()
	for i in 5: await d.touch(me if i % 2 == 0 else mate)
	d.check(d._rally.fault == "", "team touches 5: five touches fine")
	await d.touch(mate)
	d.check(d._rally.fault == "four touches", "team touches 5: the 6th is a fault")

	_g(a, "goofy.rules.block_classic", true)
	await d.frame()
	d.check(feel.block_classic and g.get_rules()["block_classic_written"], "block_classic on: PlayerFeel.block_classic written")
	_g(a, "goofy.rules.block_classic", false)
	await d.frame()
	d.check(not feel.block_classic and not g.get_rules()["block_classic_written"], "block_classic off: stock value restored")

	_g(a, "goofy.rules.double_hit_foul", false); _g(a, "goofy.rules.block_classic", true)
	await lan(true)
	var snap: Dictionary = a.gameplay_snapshot(M)
	d.check(snap["ok"] and snap["values"]["goofy.rules.double_hit_foul"] == false and snap["values"]["goofy.rules.block_classic"] == true, "LAN host: rules in the snapshot")
	await d.new_rally()
	var h2: bool = await d.touch(me); var h3: bool = await d.touch(me)
	d.check(h2 and h3, "LAN host: host enforces its own rules (double touch allowed)")
	await lan(false)
	d.check(g.get_rules()["double_hit_foul"] == true and not feel.block_classic, "LAN client before the host's values: stock rules, stock block")
	a.gameplay_apply_snapshot(M, {"values": {"goofy.rules.double_hit_foul": false, "goofy.rules.max_team_touches": 6, "goofy.rules.block_classic": true}})
	await d.frame()
	st = g.get_rules()
	d.check(not st["double_hit_foul"] and st["max_team_touches"] == 6 and feel.block_classic, "LAN client: the host's rules + block_classic win")
	var cw: int = int(st["count_writes"]); var cc: int = int(st["consecutive_clears"])
	await d.new_rally()
	await d.touch(me); await d.touch(me)
	st = g.get_rules()
	d.check(int(st["count_writes"]) == cw and int(st["consecutive_clears"]) == cc, "LAN client: never patches the rally (the host judges)")

	await public_lobby()
	st = g.get_rules()
	d.check(st["stock"] and not feel.block_classic and is_equal_approx(Engine.time_scale, 1.0), "public lobby: stock rules, block_classic restored, time scale stock")
	await offline()
	d.check(feel.block_classic and not g.get_rules()["double_hit_foul"], "offline again: own rules re-applied")


func notices() -> Array:
	var ui = api().get_node_or_null("JVSettingsUI")
	return ui.get_info()["notices"] if ui else []

func _missing_mods(a: Object) -> void:
	var g: Node = d.goofy()
	await offline()
	a.gameplay_set(M, "gstest.pace", 1.8)
	var man: Dictionary = a.gameplay_manifest(M)
	var mods: Dictionary = man.get("mods", {})
	d.check(mods.get("gstest") == "2" and mods.get("goofy") == "1" and mods.has("shots"), "manifest: gameplay mods + versions (gstest schema 2, goofy list 1 / settings schema 2, shots) %s" % str(mods))
	d.check(not mods.has("textures") and not mods.has("models") and not mods.has("loader") and not mods.keys().any(func(k): return String(k).begins_with("core_")), "manifest: texture / cosmetic / core UI libraries never listed")
	d.check(a.gameplay_mod_active_for(M, "gstest") and not a.gameplay_mod_active_for(M, "nope"), "offline: own installed mods active")
	d.check(a.gameplay_manifest_bytes(M).is_empty(), "offline: no manifest bytes (sync closed)")
	await lan(true)
	var own: PackedByteArray = a.gameplay_manifest_bytes(M)
	d.check(own.get_string_from_utf8().begins_with("JVGM 1\nloader=") and own.get_string_from_utf8().contains("gstest=2") and not own.get_string_from_utf8().contains("!"), "lobby: manifest bytes (JVGM 1), nothing disabled yet")
	var full := "JVGM 1\nloader=1\n"
	for k in mods: full += "%s=%s\n" % [k, mods[k]]

	var r: Dictionary = a.gameplay_set_peer_manifest(M, "p2", full, "Pat")
	d.check(r["ok"] and r["missing"].is_empty(), "Pat has every gameplay mod")
	a.gameplay_set_peer_manifest(M, "p3", full, "Quinn")
	var ms: Dictionary = a.gameplay_match_start(M)
	d.check(ms["ok"] and ms["disabled"].is_empty() and "gstest" in ms["active"] and "goofy" in ms["active"], "3 players, all have the mods: everything on")
	d.check(a.gameplay_mod_active_for(M, "gstest") and a.gameplay_mod_active_for(M, "gstest", "p2") and is_equal_approx(a.gameplay_get(M, "gstest.pace"), 1.8), "match: active for every player, host's value")
	d.check(not a.gameplay_set(M, "gstest.pace", 1.2)["ok"] and a.gameplay_info(M, "gstest.pace")["locked"], "settings locked mid-match")
	d.check(a.gameplay_match_end(M) and a.gameplay_set(M, "gstest.pace", 1.8)["ok"], "match end: unlocked")

	var n0 := notices().size()
	r = a.gameplay_set_peer_manifest(M, "p3", "JVGM 1\nloader=0\ngoofy=1.0\nshots=%s\n" % mods["shots"], "Quinn")
	d.check(r["ok"] and r["missing"].get("gstest") == "missing" and not r["late"], "Quinn lacks gstest (%s)" % str(r["missing"]))
	d.check(notices().size() == n0 + 1 and String(notices()[-1]).contains("Quinn") and String(notices()[-1]).contains("jvm_loader"), "host notified: Quinn's texture loader is not running")
	d.check(a.gameplay_mod_active_for(M, "gstest") and a.gameplay_match_status(M)["would_disable"].has("gstest"), "before match start: still on, status says it would be disabled")
	ms = a.gameplay_match_start(M)
	d.check(ms["ok"] and ms["disabled"].has("gstest") and ms["disabled"]["gstest"][0]["name"] == "Quinn" and "goofy" in ms["active"], "match start: gstest off for everyone (Quinn lacks it), goofy on")
	d.check(String(notices()[-1]).contains("gstest") and String(notices()[-1]).contains("Quinn"), "host notified who is missing it: %s" % notices()[-1])
	d.check(not a.gameplay_mod_active_for(M, "gstest") and not a.gameplay_mod_active_for(M, "gstest", "p2") and not a.gameplay_mod_active_for(M, "gstest", "p3"), "shared check: off for every player, host included")
	d.check(is_equal_approx(a.gameplay_get(M, "gstest.pace"), 1.0) and a.gameplay_info(M, "gstest.pace")["source"] == "stock", "host runs stock for gstest")
	var snap: Dictionary = a.gameplay_snapshot(M)
	d.check(snap["ok"] and not snap["values"].keys().any(func(k): return String(k).begins_with("gstest.")) and snap["values"].has("goofy.time_scale"), "snapshot leaves gstest out (stock on every client)")
	d.check(a.gameplay_manifest_bytes(M).get_string_from_utf8().contains("!gstest=1"), "host's list carries the decision (!gstest)")
	var pm: Dictionary = a.gameplay_player_mods(M)
	d.check(pm["p3"]["name"] == "Quinn" and pm["p3"]["missing"].has("gstest") and pm["p3"]["loader_ok"] == false and pm["p2"]["missing"].is_empty() and String(pm["local"]["inactive"].get("gstest", "")).begins_with("off for this match: p3"), "gameplay_player_mods: who lacks what, host's own status")
	d.check(not a.gameplay_set(M, "gstest.pace", 1.2)["ok"], "locked mid-match")

	r = a.gameplay_set_peer_manifest(M, "p4", "JVGM 1\nloader=1\n", "Robin")
	d.check(r["ok"] and r["late"] and a.gameplay_mod_active_for(M, "goofy"), "late joiner without goofy: goofy stays on until the next match start")
	var st2: Dictionary = a.gameplay_match_status(M)["disabled"]
	d.check(st2.keys() == ms["disabled"].keys() and not st2.has("goofy"), "mid-match decision unchanged (%s)" % str(st2.keys()))
	a.gameplay_match_end(M)
	d.check(a.gameplay_mod_active_for(M, "gstest") and is_equal_approx(a.gameplay_get(M, "gstest.pace"), 1.8), "match end: gstest back on with the host's value")
	ms = a.gameplay_match_start(M)
	d.check(ms["disabled"].has("goofy") and ms["disabled"].has("gstest") and ms["disabled"]["goofy"][0]["name"] == "Robin", "next match start: goofy off too (Robin)")
	await d.wait(0.8)
	d.check(not g.is_enabled() and g.get_status()["features"]["match_off"] and is_equal_approx(Engine.time_scale, 1.0), "goofy reverted to stock for the match (time scale %.2f)" % Engine.time_scale)
	d.check(not a.can("goofy", "game.rules") and a.why_not("goofy", "game.rules").contains("off for this match"), "core refuses a disabled mod's gameplay calls")
	a.gameplay_match_end(M)
	await d.wait(0.8)
	d.check(g.is_enabled() and is_equal_approx(Engine.time_scale, 0.75), "match end: goofy back (time scale %.2f)" % Engine.time_scale)
	d.check(not a.gameplay_set_peer_manifest(M, "p5", "JVGM 1\nBad Mod=1\n", "")["ok"] and not a.gameplay_set_peer_manifest(M, "p5", 12, "")["ok"], "bad mod lists rejected")
	d.check(a.gameplay_remove_peer(M, "p4") and not a.gameplay_player_mods(M).has("p4"), "remove_peer")

	await lan(false)
	d.check(not a.gameplay_set_peer_manifest(M, "p2", full, "")["ok"] and not a.gameplay_match_start(M)["ok"], "clients do not collect lists or decide")
	d.check(not a.gameplay_player_mods(M).has("p2"), "becoming a client forgets the players' lists")
	a.gameplay_apply_snapshot(M, {"values": {"gstest.pace": 1.9, "goofy.time_scale": 0.8}})
	d.check(not a.gameplay_mod_active_for(M, "gstest"), "client: waiting for the host's list = off for the shared check (fail closed)")
	var hb := "JVGM 1\nloader=1\n"
	for k in mods: hb += "%s=%s\n" % [k, mods[k]]
	r = a.gameplay_set_host_manifest(M, hb + "!gstest=1\n")
	d.check(r["ok"] and r["disabled"].has("gstest") and "goofy" in r["active"] and r["missing_here"].is_empty(), "client: the host disabled gstest for the match")
	d.check(is_equal_approx(a.gameplay_get(M, "gstest.pace"), 1.0) and is_equal_approx(a.gameplay_get(M, "goofy.time_scale"), 0.8) and not a.gameplay_mod_active_for(M, "gstest"), "client: gstest stock, goofy the host's value")
	var n2 := notices().size()
	r = a.gameplay_set_host_manifest(M, hb.replace("gstest=2", "gstest=3") + "!gstest=1\n")
	d.check(r["missing_here"].size() == 1 and notices().size() == n2 + 1 and String(notices()[-1]).begins_with("you do not have gstest"), "client with another major version: told the host turned it off for everyone")
	r = a.gameplay_set_host_manifest(M, "JVGM 1\nloader=1\ngoofy=1\n")
	d.check(r["disabled"].has("gstest") and not a.gameplay_mod_active_for(M, "gstest"), "client: the host lacks gstest -> off here too")
	d.check(a.gameplay_player_mods(M).has("host"), "client: the host's list is visible")
	await offline()
	d.check(is_equal_approx(a.gameplay_get(M, "gstest.pace"), 1.8) and a.gameplay_mod_active_for(M, "gstest") and a.gameplay_match_status(M)["disabled"].is_empty(), "offline again: everything back on")

	var ld: Node = d.get_node_or_null("/root/JVLoader")
	if ld == null: d.check(false, "loader node present for the notice test"); return
	ld.name = "JVLoaderStopped"
	var n1 := notices().size()
	await lan(false)
	d.check(notices().size() == n1 + 1 and String(notices()[-1]).begins_with("your texture loader (jvm_loader) is not running") and a.gameplay_manifest(M)["loader_ok"] == false, "own loader not running: this player is notified on joining (%s)" % str(notices().slice(n1)))
	await offline()
	ld.name = "JVLoader"
	await lan(true)
	d.check(notices().size() == n1 + 1, "loader running: no notice")
	await offline()

func _g(a: Object, id: String, v) -> void:
	var r: Dictionary = a.gameplay_set("goofy", id, v)
	if not r["ok"]: d.check(false, "set %s: %s" % [id, r["error"]])
func _reset(a: Object, id: String) -> void: a.gameplay_reset("goofy", id)
