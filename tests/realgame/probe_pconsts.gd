extends Node






const PG := "res://game/player/player.gd"
var mode := OS.get_environment("JV_PC_MODE")
var fails := 0
func p(m: String) -> void: print("PROBE ", m)
func check(c: bool, what: String) -> void:
	if not c: fails += 1
	print(("PASS " if c else "FAIL ") + what)
func wait(s: float) -> void: await get_tree().create_timer(s).timeout

func to_menu() -> Node:
	var m: Node = null
	for i in 120:
		await wait(1.0)
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"): m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_try_freeplay") and m.get("player") != null:
			await wait(2.0); return m
	return null

func start_match(m: Node) -> Node:
	var gc = get_node("/root/GameConfig"); gc.set("cpu_fill", 0)
	if gc.get("match_intro") != null: gc.set("match_intro", false)
	m.call("_go_to_match", 2)
	for i in 60:
		await wait(0.5)
		var s := get_tree().current_scene
		if s and s.scene_file_path == "res://game/main/main.tscn" and s.get("player") != null: return s
	return null

func all_stock(P: Script, stock: Dictionary) -> bool:
	for n in stock:
		if float(P.get(n)) != float(stock[n]): return false
	return stock.size() == 34

func _ready() -> void:
	var meta: Dictionary = Engine.get_meta("jvm_player_consts", {})
	p("boot %s" % meta)
	var m := await to_menu()
	var P: GDScript = load(PG)
	var g: Node = get_node_or_null("/root/JVGoofyMods")
	if mode == "perf": await perf(m, P); return
	check(m != null and g != null, "menu reached, Goofy loaded")
	check(bool(meta.get("mounted", false)), "overlay mounted at boot (%s, %.1f ms)" % [meta.get("reason", ""), float(meta.get("ms", -1))])
	check(not P.get_script_constant_map().has("SET_DUMP_RISE_TAP"), "player.gd consts are static vars now")
	var st: Dictionary = g.player_consts_get()
	var stock: Dictionary = st["stock"]
	check(st["available"] and all_stock(P, stock) and not st["locked"], "menu: available, all 34 stock, unlocked (%s)" % st["reason"])
	var a: Object = Engine.get_singleton("JVModAPI")
	var caps := PackedStringArray()
	for c in a.get_capabilities(): if c["status"] != "planned": caps.append(c["id"])
	a.register_mod("rg_probe", "1.2", caps)
	check(a.player_consts_set("rg_probe", "SET_DUMP_RISE_TAP", 5.0)["ok"] and a.player_consts_set("rg_probe", "BLOCK_TURN", 4.0)["ok"] and a.player_consts_set("rg_probe", "SPIKE_TURN_FLOOR", 0.5)["ok"], "menu: CORE API edits accepted")
	check(float(P.get("SET_DUMP_RISE_TAP")) == 2.8, "menu: edits saved, not live yet")
	var main := await start_match(m)
	check(main != null, "CPU match started")
	if main == null: get_tree().quit(1); return
	await wait(1.0)
	var players: Array = main.find_children("*", "CharacterBody3D", true, false).filter(func(n): return n.get_script() == P)
	st = g.player_consts_get()
	check(st["locked"] and st["applied"], "match: locked + applied (notice '%s')" % st["notice"])
	check(float(P.get("SET_DUMP_RISE_TAP")) == 5.0 and float(P.get("BLOCK_TURN")) == 4.0 and players.size() > 0 and players.all(func(pl): return pl.set_dump_rise(0.0) == 5.0) and P.spike_turn_keep(3.0) == 0.5,
		"match: start-of-match set live on all %d players (set_dump_rise(0)=%s)" % [players.size(), players[0].set_dump_rise(0.0) if players.size() else "?"])
	var balls := main.find_children("*", "", true, false).filter(func(n): return n.get_script() != null and String(n.get_script().resource_path) == "res://game/ball/ball.gd")
	var b0: Vector3 = balls[0].global_position if balls.size() else Vector3.ZERO

	var r1: Dictionary = a.player_consts_set("rg_probe", "SET_DUMP_RISE_TAP", 9.0)
	var r2: Dictionary = a.player_consts_reset("rg_probe", "*")
	var r3: Dictionary = g.player_consts_set("BLOCK_TURN", 1.0, "goofy-ui")
	check(not r1["ok"] and "locked" in str(r1["error"]), "mid-match CORE API set rejected: %s" % r1["error"])
	check(not r2["ok"] and not r3["ok"], "mid-match reset + Goofy UI write rejected")
	check(float(P.get("SET_DUMP_RISE_TAP")) == 5.0 and float(P.get("BLOCK_TURN")) == 4.0 and players[0].set_dump_rise(0.0) == 5.0, "mid-match values unchanged")
	await wait(8.0)
	var b1: Vector3 = balls[0].global_position if balls.size() else Vector3.ZERO
	check(b0.distance_to(b1) > 0.5, "match runs with the edits (ball moved %.2f)" % b0.distance_to(b1))
	check(int(g.player_consts_get()["rejected"]) >= 3, "rejections logged (%d)" % g.player_consts_get()["rejected"])
	get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn")
	await wait(3.0)
	st = g.player_consts_get()
	check(not st["locked"] and all_stock(P, stock), "back in the menu: unlocked, all 34 exactly stock")
	check(a.player_consts_set("rg_probe", "SET_DUMP_RISE_TAP", 3.0)["ok"], "menu again: edits accepted")
	a.player_consts_reset("rg_probe", "*")
	p("RESULT fails=%d" % fails)
	get_tree().quit(1 if fails else 0)


var _t := []; var _proc := []; var _phys := []; var _last := 0; var _on := false
func _process(_d: float) -> void:
	if not _on: return
	var now := Time.get_ticks_usec()
	if _last > 0: _t.append((now - _last) / 1000.0)
	_last = now
	_proc.append(Performance.get_monitor(Performance.TIME_PROCESS) * 1000.0)
	_phys.append(Performance.get_monitor(Performance.TIME_PHYSICS_PROCESS) * 1000.0)

func stats(a: Array) -> String:
	var s := a.duplicate(); s.sort()
	var sum := 0.0
	for v in s: sum += v
	var hitch := s.filter(func(v): return v > 16.7).size()
	return "n=%d avg=%.3f p50=%.3f p99=%.3f max=%.3f hitch16.7=%d" % [s.size(), sum / max(1, s.size()), s[s.size() / 2], s[int(s.size() * 0.99)], s[-1], hitch]

func perf(m: Node, P: GDScript) -> void:
	p("perf variant=%s static=%s value=%s" % [OS.get_environment("JV_PC_VARIANT"), not P.get_script_constant_map().has("SET_DUMP_RISE_TAP"), P.get("SET_DUMP_RISE_TAP")])
	var main := await start_match(m)
	if main == null: p("no match"); get_tree().quit(1); return
	await wait(3.0)
	var secs := float(OS.get_environment("JV_PC_SECS")) if OS.get_environment("JV_PC_SECS") != "" else 30.0
	_on = true
	await wait(secs)
	_on = false
	p("perf frame_ms %s" % stats(_t))
	p("perf process_ms %s" % stats(_proc))
	p("perf physics_ms %s" % stats(_phys))
	p("perf fps_monitor=%.1f" % Performance.get_monitor(Performance.TIME_FPS))

	var players: Array = main.find_children("*", "CharacterBody3D", true, false).filter(func(n): return n.get_script() == P)
	p("perf in-match SET_DUMP_RISE_TAP=%s players=%d" % [P.get("SET_DUMP_RISE_TAP"), players.size()])
	if players.size():
		var pl = players[0]
		var best_a := 1e9; var best_b := 1e9; var best_c := 1e9
		for rep in 7:
			var t0 := Time.get_ticks_usec()
			for i in 200000: pl.set_dump_rise(0.0)
			best_a = min(best_a, (Time.get_ticks_usec() - t0) * 1000.0 / 200000.0)
			t0 = Time.get_ticks_usec()
			for i in 200000: P.spike_turn_keep(3.0)
			best_b = min(best_b, (Time.get_ticks_usec() - t0) * 1000.0 / 200000.0)
			t0 = Time.get_ticks_usec()
			for i in 200000: pl.get_instance_id()
			best_c = min(best_c, (Time.get_ticks_usec() - t0) * 1000.0 / 200000.0)
		p("perf micro ns/call set_dump_rise=%.1f spike_turn_keep=%.1f baseline_call=%.1f" % [best_a, best_b, best_c])
	get_tree().quit(0)
