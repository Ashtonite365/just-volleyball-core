extends Node





const PG := "res://game/player/player.gd"
var mode := OS.get_environment("JV_CB_MODE")
func p(m: String) -> void: print("PROBE ", m)

func _init() -> void:
	p("init mode=%s cached player.gd=%s player.tscn=%s" % [mode, ResourceLoader.has_cached(PG), ResourceLoader.has_cached("res://game/player/player.tscn")])
	if mode != "patched" and mode != "perplayer": return
	var t0 := Time.get_ticks_usec()
	var pk := PCKPacker.new(); var pck := OS.get_user_data_dir().path_join("cb_player.pck")
	pk.pck_start(pck); pk.add_file("res://game/player/player.gdc", ProjectSettings.globalize_path("res://mods/cb/player_patched.gdc"))
	if mode == "perplayer": pk.add_file("res://game/fx/hitbox_view.gdc", ProjectSettings.globalize_path("res://mods/cb/hitbox_view_patched.gdc"))
	pk.flush()
	var ok := ProjectSettings.load_resource_pack(pck, true)
	p("overlay mounted=%s us=%d" % [ok, Time.get_ticks_usec() - t0])

func _ready() -> void:
	p("ready cached player.gd=%s (bootstrap _ready timing)" % ResourceLoader.has_cached(PG))
	var P: GDScript = load(PG)
	p("P is_Player_global=%s" % [P == ResourceLoader.load(PG)])
	p("P const_DIVE_DENY_TIME=%s static_get=%s" % [P.get_script_constant_map().has("DIVE_DENY_TIME"), P.get("SET_DUMP_RISE_TAP")])
	var m: Node = null
	for i in 120:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"): m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_try_freeplay") and m.get("player") != null:
			await get_tree().create_timer(3.0).timeout; break
	p("menu=%s" % [m != null and m.has_method("_go_to_match")])
	var gc = get_node("/root/GameConfig"); gc.set("cpu_fill", 0)
	if gc.get("match_intro") != null: gc.set("match_intro", false)
	m.call("_go_to_match", 2)
	var main: Node = null
	for i in 60:
		await get_tree().create_timer(1.0).timeout
		var s := get_tree().current_scene
		if s and s.scene_file_path == "res://game/main/main.tscn" and s.get("player") != null: main = s; break
	if main == null: p("no match"); get_tree().quit(1); return
	var players: Array = main.find_children("*", "CharacterBody3D", true, false).filter(func(n): return n.get_script() == P)
	p("match players=%d all_P=%s script_is_P=%s" % [players.size(), players.size() > 0, players.all(func(n): return n.get_script() == P)])
	var ball = main.find_children("*", "", true, false).filter(func(n): return n.get_script() != null and String(n.get_script().resource_path) == "res://game/ball/ball.gd")
	var b0: Vector3 = ball[0].global_position if ball.size() else Vector3.ZERO
	await get_tree().create_timer(8.0).timeout
	var b1: Vector3 = ball[0].global_position if ball.size() else Vector3.ZERO
	var pl = players[0]
	p("t+8s ball_moved=%.2f set_dump_rise(0)=%s spike_turn_keep(3)=%s" % [b0.distance_to(b1), pl.set_dump_rise(0.0), P.spike_turn_keep(3.0)])
	if mode == "patched":
		P.set("SET_DUMP_RISE_TAP", 5.0); P.set("SPIKE_TURN_FLOOR", 0.5); P.set("BLOCK_TURN", 4.0)
		p("live edit mid-match set_dump_rise(0)=%s (want 5) spike_turn_keep(3)=%s (want 0.5)" % [pl.set_dump_rise(0.0), P.spike_turn_keep(3.0)])
		await get_tree().create_timer(5.0).timeout
		P.set("SET_DUMP_RISE_TAP", 2.8); P.set("SPIKE_TURN_FLOOR", 0.15); P.set("BLOCK_TURN", 2.0)
		p("reverted set_dump_rise(0)=%s spike_turn_keep(3)=%s" % [pl.set_dump_rise(0.0), P.spike_turn_keep(3.0)])
	if mode == "perplayer":
		var hv = load("res://game/fx/hitbox_view.gd")
		p("perplayer hitbox_view compiles=%s const_map_has=%s" % [hv != null and hv.can_instantiate(), P.get_script_constant_map().has("SET_DUMP_RISE_TAP")])

		var edited := {"SET_DUMP_RISE_TAP": 5.0, "SPIKE_TURN_FLOOR": 0.5, "BLOCK_TURN": 4.0, "JUMP_SPIKE_GRACE_US": 200000}
		for i in players.size() - 1:
			for k in edited: players[i].set(k, edited[k])
		var rows := []
		for pl2 in players: rows.append("%s:%s/%s/%s" % [pl2.name, pl2.set_dump_rise(0.0), pl2.spike_turn_keep(3.0), pl2.get("JUMP_SPIKE_GRACE_US")])
		p("perplayer per-instance (rise/turnkeep/grace) %s" % ", ".join(rows))
		await get_tree().create_timer(8.0).timeout
		var b3: Vector3 = ball[0].global_position if ball.size() else Vector3.ZERO
		p("perplayer 8s later ball_moved=%.2f last_player_still_stock=%s" % [b1.distance_to(b3), players[-1].set_dump_rise(0.0) == 2.8])
		var fresh = load("res://game/player/player.tscn").instantiate()
		p("perplayer fresh instance defaults=stock %s/%s/%s" % [fresh.set_dump_rise(0.0), fresh.spike_turn_keep(3.0), fresh.get("JUMP_SPIKE_GRACE_US")]); fresh.free()
		for pl2 in players:
			for k in edited: pl2.set(k, fresh_stock(k))
		var rows2 := []
		for pl2 in players: rows2.append("%s/%s" % [pl2.set_dump_rise(0.0), pl2.spike_turn_keep(3.0)])
		p("perplayer revert-to-stock %s" % ", ".join(rows2))
	await get_tree().create_timer(5.0).timeout
	var b2: Vector3 = ball[0].global_position if ball.size() else Vector3.ZERO
	p("end ball_moved_late=%.2f players=%d score=%s" % [b1.distance_to(b2), main.find_children("*", "CharacterBody3D", true, false).filter(func(n): return n.get_script() == P).size(), str(main.get("score")) if main.get("score") != null else "?"])
	p("DONE"); get_tree().quit()


func fresh_stock(k: String):
	return {"SET_DUMP_RISE_TAP": 2.8, "SPIKE_TURN_FLOOR": 0.15, "BLOCK_TURN": 2.0, "JUMP_SPIKE_GRACE_US": 140000}[k]
