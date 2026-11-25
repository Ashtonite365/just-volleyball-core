extends Node





func p(msg: String) -> void: print("PROBE ", msg)
var host: Node = null
var pl = null
var ball = null
var sh: Node = null
var out_dir := ""

func ev_for(action: String, pressed: bool) -> InputEvent:
	for e in InputMap.action_get_events(action):
		if e is InputEventKey:
			var k := InputEventKey.new()
			k.keycode = (e as InputEventKey).keycode; k.physical_keycode = (e as InputEventKey).physical_keycode
			k.pressed = pressed
			return k
		if e is InputEventMouseButton:
			var mb := InputEventMouseButton.new()
			mb.button_index = (e as InputEventMouseButton).button_index; mb.pressed = pressed
			mb.position = get_viewport().get_visible_rect().size * 0.5
			return mb
	return null
func key(action: String, pressed: bool) -> void:
	var e := ev_for(action, pressed)
	if e: Input.parse_input_event(e)
	else: p("no key/mouse binding for " + action)


func touches() -> int:
	if sh.has_method("get_touch_count"): return int(sh.get_touch_count())
	return sh.get_applied_count() + sh.get_skipped_count()
func frames(n: int) -> void:
	for i in n: await get_tree().physics_frame
func tap(action: String, n := 3) -> void:
	key(action, true); await frames(n); key(action, false); await frames(2)
func shot(name: String) -> void:
	if out_dir == "": return
	await RenderingServer.frame_post_draw
	var vp: Viewport = ball.get_viewport() if ball else get_viewport()
	var img := vp.get_texture().get_image(); var path := out_dir.path_join(name + ".png"); img.save_png(path); p("screenshot=" + path)

	if vp != get_viewport():
		var root_img := get_viewport().get_texture().get_image(); var rp := out_dir.path_join(name + "-screen.png"); root_img.save_png(rp); p("screenshot=" + rp)

func state(tag: String) -> String:
	return "%s t=%.2f grounded=%s serving=%s own_ball=%s ball_pos=%s ball_frozen=%s contact_state=%s opp=%s last=%s applied=%s skipped=%s" % [tag, Time.get_ticks_msec() / 1000.0,
		pl.grounded(), pl.serving, pl.shared_ball_off, ball.global_position, ball.frozen, pl.contact_state, pl.contact_opportunity_kind(), sh.get_last_event(), sh.get_applied_count(), sh.get_skipped_count()]


func toss() -> bool:
	for attempt in 3:
		await tap("serve", 3)
		await frames(20)
		if pl.serving or ball.frozen: break
	p(state("after serve key"))
	key("contact", true); await frames(18); key("contact", false)
	for i in 40:
		await frames(1)
		if not ball.frozen: break
	p(state("after toss"))
	return not ball.frozen


var _held_moves: Array = []
var _air_at_touch := false
func steer(to: Vector3) -> void:
	var want: Array = []
	var d: Vector3 = to - pl.global_position; d.y = 0
	if d.length() > 0.25:
		var cam: Camera3D = ball.get_viewport().get_camera_3d()
		var f: Vector3 = -cam.global_basis.z; f.y = 0; f = f.normalized()
		var r: Vector3 = cam.global_basis.x; r.y = 0; r = r.normalized()
		var df := d.dot(f); var dr := d.dot(r)
		if absf(df) > 0.2: want.append("move_forward" if df > 0 else "move_back")
		if absf(dr) > 0.2: want.append("move_right" if dr > 0 else "move_left")
	for a in _held_moves: if not want.has(a): key(a, false)
	for a in want: if not _held_moves.has(a): key(a, true)
	_held_moves = want
func stop_moves() -> void:
	for a in _held_moves: key(a, false)
	_held_moves = []



func air_hit(hold: Array, label: String, air := true) -> void:
	var b: Node3D = ball
	var n0: int = sh.get_applied_count() + sh.get_skipped_count()
	var seq0: int = touches()
	var prev_y := b.global_position.y
	var jumped := false
	for i in 240:
		await frames(1)
		var v: Vector3 = b.get("vel")
		var y := b.global_position.y

		var t_hit := 0.0
		var g := 11.0
		var hit_y := 3.1 if air else 1.6
		var disc := v.y * v.y + 2.0 * g * (y - hit_y)
		if disc > 0: t_hit = (v.y + sqrt(disc)) / g
		var spot: Vector3 = b.global_position + Vector3(v.x, 0, v.z) * t_hit
		var away: Vector3 = pl.global_position - spot; away.y = 0
		steer(spot + (away.normalized() * 0.35 if away.length() > 0.01 else Vector3.ZERO))
		if OS.get_environment("JV_TRACE") != "" and i % 6 == 0: p("pre %s i=%d ball=%s v=%s spot=%s player=%s t_hit=%.2f" % [label, i, b.global_position, v, spot, pl.global_position, t_hit])
		if v.y < 0 and t_hit < 0.55: break
		prev_y = y
	stop_moves()
	if air:
		key("jump", true); await frames(3); key("jump", false)
	for a in hold: key(a, true)
	var pressed := false
	var hit := false
	for i in 120:
		await frames(1)
		if OS.get_environment("JV_TRACE") != "" and i % 2 == 0: p("trace %s i=%d ball=%s player=%s grounded=%s reach=%s cs=%s" % [label, i, b.global_position, pl.global_position, pl.grounded(), pl._swing_reach_check(), pl.contact_state])
		if touches() != seq0: hit = true; _air_at_touch = not pl.grounded(); break
		if not pressed and (not pl.grounded() or not air):
			key("contact", true); pressed = true
		elif pressed and pl._swing_reach_check():
			key("contact", false); pressed = false
	if pressed: key("contact", false)
	await frames(4)
	for a in hold: key(a, false)
	p("case=%s keys=%s touched=%s mod_events=%d | %s" % [label, hold, hit, sh.get_applied_count() + sh.get_skipped_count() - n0, state("after hit")])

func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
	var m: Node = null
	for i in 150:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"):
			m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_try_freeplay") and m.get("player") != null:
			await get_tree().create_timer(2.0).timeout
			break
	process_mode = Node.PROCESS_MODE_ALWAYS
	InputCollector.dev_assume_focus = true
	sh = get_node_or_null("/root/JVExtraShots")
	p("menu reached; shots=%s" % [sh != null])
	m.call("_try_freeplay"); await get_tree().create_timer(5.0).timeout
	for n in get_tree().root.find_children("*", "Node", true, false):
		if n == m or n.get("camera_rig") == null: continue
		if n.get("player") is Node3D and n.get("ball") is Node3D and (n.get("player") as Node3D).is_visible_in_tree():
			host = n; break
	if host == null: p("no practice host"); get_tree().quit(3); return
	pl = host.get("player"); ball = host.get("ball")
	p("host=%s player=%s ball=%s gate=%s" % [host.get_path(), pl.get_path(), ball.get_path(), get_node("/root/JVModGate").get_mode() if get_node_or_null("/root/JVModGate") else "?"])
	var menu_pl = m.get("player")
	p("menu player serving=%s (the hidden menu-roam player)" % [menu_pl.get("serving") if menu_pl else "?"])
	if sh.has_method("set_debug_visible"): sh.set_debug_visible(true)
	await frames(30)



	var cases := (OS.get_environment("JV_CASES") if OS.get_environment("JV_CASES") != "" else "topspin:move_forward:air:topspin,curveball_r:move_right:air:curveball,curveball_l:move_left:air:curveball,float:move_back:air:float,jump_serve:sprint:air:jump_serve,float_serve:sprint+move_back:air:float_serve,ground_serve:sprint:ground:high_serve|low_serve|fast_serve,plain_air::air:none,plain_ground::ground:none").split(",")
	var passed := 0; var failed: Array = []
	for c in cases:
		var parts := c.split(":")
		var label: String = parts[0]; var keys: Array = [] if parts[1] == "" else Array(parts[1].split("+"))
		var kind: String = parts[2] if parts.size() > 2 else "air"
		var expect: String = parts[3] if parts.size() > 3 else ""
		var air := kind != "ground"
		var ok := false
		var a0: int = sh.get_applied_count()
		for attempt in 3:
			if not await toss(): p("case=%s toss failed" % label); await get_tree().create_timer(2.0).timeout; continue
			if kind == "rally":
				var s0: int = touches()
				await air_hit([], label + "/first", false)
				if touches() == s0: p("case=%s attempt %d: no first touch, retry" % [label, attempt + 1]); await get_tree().create_timer(3.0).timeout; continue
				a0 = sh.get_applied_count()
			var seq0: int = touches()
			await air_hit(keys, label, air)
			if touches() != seq0: ok = true; break
			p("case=%s attempt %d: no touch, retry" % [label, attempt + 1])
			await get_tree().create_timer(3.0).timeout
			a0 = sh.get_applied_count()
		if sh.has_method("get_debug_text"): p("case=%s debugger:\n%s" % [label, sh.get_debug_text()])
		await shot("realinput-" + label)
		if expect != "":
			var last: String = sh.get_last_event()
			var good := ok and (air == _air_at_touch or kind == "ground")
			if expect == "none": good = good and sh.get_applied_count() == a0
			else:
				var any := false
				for e in expect.split("|"): any = any or last.begins_with("applied " + e + " ") or last == "applied " + e
				good = good and sh.get_applied_count() == a0 + 1 and any
			p("VERDICT case=%s %s expect=%s touched=%s airborne_at_touch=%s last=%s" % [label, "PASS" if good else "FAIL", expect, ok, _air_at_touch, last])
			if good: passed += 1
			else: failed.append(label)
		await get_tree().create_timer(3.0).timeout
	p("RESULT %s passed=%d failed=%s" % ["ALL PASS" if failed.is_empty() else "FAIL", passed, str(failed)])
	p("done")
	get_tree().quit(0 if failed.is_empty() else 1)
