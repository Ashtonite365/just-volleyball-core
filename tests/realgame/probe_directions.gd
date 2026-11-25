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
	var air_frames := 0
	for i in 120:
		await frames(1)
		if OS.get_environment("JV_TRACE") != "" and i % 2 == 0: p("trace %s i=%d ball=%s player=%s grounded=%s reach=%s cs=%s" % [label, i, b.global_position, pl.global_position, pl.grounded(), pl._swing_reach_check(), pl.contact_state])
		if touches() != seq0: hit = true; _air_at_touch = not pl.grounded(); break
		if not pressed and (not pl.grounded() or not air):
			air_frames += 1

			if press_mode == "full" or (press_mode == "mid" and air_frames >= mid_delay) or (press_mode == "tap" and pl._swing_reach_check()):
				key("contact", true); pressed = true
		elif pressed and pl._swing_reach_check():
			key("contact", false); pressed = false
	if pressed: key("contact", false)
	await frames(4)
	for a in hold: key(a, false)
	p("case=%s keys=%s touched=%s mod_events=%d | %s" % [label, hold, hit, sh.get_applied_count() + sh.get_skipped_count() - n0, state("after hit")])



var _m := {}
func follow_flight() -> Dictionary:
	var b: Node3D = ball
	var start: Vector3 = b.global_position
	var v0: Vector3 = b.get("vel")
	var pred: Vector3 = b.call("_predict_land_curved") if b.has_method("_predict_land_curved") else Vector3.INF
	var apex := start.y; var land := Vector3.INF; var prev := start; var net_y := INF
	var pv: Vector3 = v0
	var rad: float = float(b.get("PHYS").get("radius")) if b.get("PHYS") != null else 0.54
	for i in 400:
		await frames(1)
		var q := b.global_position
		var vv: Vector3 = b.get("vel")
		apex = maxf(apex, q.y)

		if prev.y > rad + 0.001 and q.y <= rad + 0.02:
			land = Vector3(q.x, 0, q.z); break
		if i > 3 and pv.y < -0.5 and vv.y > pv.y + 2.0 and q.y < 1.2:
			land = Vector3(q.x, 0, q.z); break
		pv = vv
		if signf(prev.z) != signf(q.z) and absf(q.z - prev.z) > 1e-4:
			var t := prev.z / (prev.z - q.z); net_y = lerpf(prev.y, q.y, t)
		prev = q
	return {"from": start, "v0": v0, "speed": v0.length(), "apex": apex, "land": land, "pred": pred, "net_y": net_y}

func walk_to(spot: Vector3) -> void:
	for i in 240:
		steer(spot); await frames(1)
		if Vector3(pl.global_position.x - spot.x, 0, pl.global_position.z - spot.z).length() < 0.4: break
	stop_moves()



func box_clamp(a: Vector3) -> Vector3:
	var cx: float = ball.get("court_center_x"); var hw: float = maxf(1.0, ball.get("court_half_w")); var hl: float = maxf(2.0, ball.get("court_half_l"))
	var sd := 1.0 if a.z >= 0 else -1.0
	var z0 := 0.3 if sd > 0 else -hl; var z1 := hl if sd > 0 else -0.3
	return Vector3(clampf(a.x, cx - hw, cx + hw), 0, clampf(a.z, z0, z1))

func flat(v: Vector3) -> Vector3: return Vector3(v.x, 0, v.z).normalized()
func cam() -> Camera3D: return host.get("camera_rig").get("camera")


var walk_home := false
var press_mode := "full"
var mid_delay := 10
var shot_after_touch := ""
var bar_seen := {}
func _shot_in_flight(name: String) -> void:
	await frames(14)
	bar_seen[name] = {"bar": sh.get_spin_bar() if sh.has_method("get_spin_bar") else {}, "box": sh.get_range_box() if sh.has_method("get_range_box") else {}, "float_serve": bool(ball.get("float_serve")), "spread": float(ball.get("_predict_spread"))}
	await shot(name)
var home_spot := Vector3.INF
func one(keys: Array, label: String) -> Dictionary:
	var fallback := {}
	for attempt in 6:
		if walk_home:
			var hs0: float = float(pl.get("team_side")) if int(pl.get("team_side")) != 0 else 1.0
			await walk_to(Vector3(0, 0, hs0 * 5.0))
			if Vector3(pl.global_position.x, 0, pl.global_position.z - hs0 * 5.0).length() > 1.0:
				p("walk home missed: player at %s, placing at the serve spot" % pl.global_position)
				(pl as Node3D).global_position = Vector3(0, pl.global_position.y, hs0 * 5.0)
		if home_spot != Vector3.INF and attempt > 0: await walk_to(home_spot)
		if not await toss(): await get_tree().create_timer(1.5).timeout; continue
		var t0 := touches()
		var a0: int = sh.get_applied_count()
		var info := {}

		var watcher := func():
			for i in 600:
				await get_tree().physics_frame
				if touches() != t0:
					if shot_after_touch != "": _shot_in_flight(shot_after_touch)
					var c := cam()
					info.merge({"cam_fwd": flat(-c.global_basis.z), "cam_right": flat(c.global_basis.x),
						"pl_fwd": flat((pl.get("visual") as Node3D).global_basis.z if pl.get("visual") else -(pl as Node3D).global_basis.z),
						"aim": Vector3(pl.aim_point.x, 0, pl.aim_point.z), "contact": ball.global_position, "last": sh.get_last_event(), "applied": sh.get_applied_count() - a0, "spin": sh.get_last_spin(), "flt": sh.get_last_float(), "pl_pos": (pl as Node3D).global_position, "team": int(pl.get("team_side"))})
					info.merge(await follow_flight())
					return
		watcher.call()
		await air_hit(keys, label, true)
		for i in 500:
			if info.has("land"): break
			await frames(1)
		if info.has("land") and info["land"] != Vector3.INF:

			if keys.is_empty() or int(info.get("applied", 0)) > 0: return info
			fallback = info
			fallback["last"] = "no air hit (body touch, the game's own shot)"
		await get_tree().create_timer(2.0).timeout
	return fallback

func fmt(v) -> String:
	if v is Vector3: return "(%.2f,%.2f,%.2f)" % [v.x, v.y, v.z]
	if v is float: return "%.3f" % v
	return str(v)
func dump(tag: String, r: Dictionary) -> void:
	var s := tag
	for k in ["last", "speed", "apex", "net_y", "contact", "aim", "land", "pred", "cam_fwd", "cam_right", "pl_fwd"]:
		if r.has(k): s += " %s=%s" % [k, fmt(r[k])]
	p(s)

var verdicts: Array = []
var cursor_world := Vector3.INF
func _physics_process(_d: float) -> void:
	if cursor_world != Vector3.INF and host != null and cam() != null:
		InputCollector.dev_cursor = cam().unproject_position(cursor_world)
func verdict(name: String, ok: bool, detail: String) -> void:
	verdicts.append([name, ok]); p("VERDICT %s %s %s" % [name, "PASS" if ok else "FAIL", detail])

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
	m.call("_try_freeplay"); await get_tree().create_timer(5.0).timeout
	for n in get_tree().root.find_children("*", "Node", true, false):
		if n == m or n.get("camera_rig") == null: continue
		if n.get("player") is Node3D and n.get("ball") is Node3D and (n.get("player") as Node3D).is_visible_in_tree():
			host = n; break
	if host == null: p("no practice host"); get_tree().quit(3); return
	pl = host.get("player"); ball = host.get("ball")
	var rig = host.get("camera_rig")
	p("host=%s camera=%s mode=%s yaw=%.3f" % [host.get_path(), cam().get_path(), rig.mode, rig.get("_yaw")])
	if sh.has_method("set_debug_visible"): sh.set_debug_visible(true)
	await frames(30)
	var only := OS.get_environment("JV_PARTS")




	if only == "" or only.contains("yaw"):
		walk_home = true
		var ref_float := {}


		var views := (OS.get_environment("JV_VIEWS") if OS.get_environment("JV_VIEWS") != "" else "dynamic,broadcast,static,action0,action90").split(",")
		for deg in views:
			var mode_id := 1 if deg == "dynamic" else (2 if deg == "broadcast" else (3 if deg == "static" else 0))
			for i in 8:
				if rig.mode == mode_id: break
				rig.call("cycle_mode"); await frames(5)
			var base: float = rig.get("_yaw")
			p("view %s: camera mode %s" % [deg, rig.mode])
			for c in [["W", ["move_forward"]], ["A", ["move_left"]], ["D", ["move_right"]], ["S", ["move_back"]], ["S+A", ["move_back", "move_left"]], ["S+D", ["move_back", "move_right"]]]:
				var hs := signf(pl.global_position.z) if absf(pl.global_position.z) > 0.3 else 1.0
				await walk_to(Vector3(0, 0, hs * 5.0))
				if mode_id == 0:
					cursor_world = Vector3.INF; InputCollector.dev_cursor = Vector2.INF
					rig.set("_yaw", base + deg_to_rad(90.0 if deg == "action90" else 0.0)); rig.set("_pitch", -0.25)
				else:
					cursor_world = Vector3(1.5, 0, -hs * 5.0)
				await frames(20)
				var r: Dictionary = await one(c[1], "yaw%s-%s" % [deg, c[0]])
				dump("yaw=%s keys=%s" % [deg, c[0]], r)
				var name := "yaw%s_%s" % [deg, c[0]]
				if c[0] == "D" or c[0] == "S+A": await shot("dir-yaw%s-%s" % [deg, c[0].replace("+", "")])
				if r.is_empty(): verdict(name, false, "no shot"); continue

				var rx := (1.0 if "D" in c[0] else 0.0) - (1.0 if "A" in c[0] else 0.0)
				var ry := (1.0 if "S" in c[0] else 0.0) - (1.0 if "W" in c[0] else 0.0)
				var walk: Vector3 = Vector3(r["cam_right"]) * rx + Vector3(r["cam_fwd"]) * -ry
				if walk.length() > 1.0: walk = walk.normalized()
				var pp: Vector3 = r["pl_pos"]; var to_aim := Vector3(r["aim"].x - pp.x, 0, r["aim"].z - pp.z)


				var net_dir: Vector3 = Vector3(0, 0, -float(r["team"]) if r["team"] != 0 else -signf(pp.z))
				var net_right := net_dir.cross(Vector3.UP)
				var fx := walk.dot(net_right); var fy := -walk.dot(net_dir)
				var exp_shot := "topspin" if -fy > 0.5 else ("float" if fy > 0.5 and fy >= absf(fx) else ("curveball" if absf(fx) > 0.5 else "none"))
				var exp_side := 1 if fx > 0.5 else (-1 if fx < -0.5 else 0)
				var last := String(r["last"])
				var ok := last.begins_with("applied " + exp_shot)
				var detail := "keys %s walk you %s = hitting frame (right %+.2f, toward net %+.2f) -> expect %s%s; got '%s'" % [c[0], fmt(walk), fx, -fy, exp_shot, ("" if exp_side == 0 else (" right" if exp_side > 0 else " left")), last]
				if ok and exp_shot == "topspin":
					var err: float = (Vector3(r["land"]) - Vector3(r["aim"])).length()
					ok = err < 0.25; detail += ", landed %.2f m from the cursor" % err
				elif ok and exp_shot == "curveball":
					var sp: Dictionary = r["spin"]
					var tgt: Vector3 = sp.get("target", Vector3.INF); var aimv: Vector3 = sp.get("aim", r["aim"]); aimv.y = 0
					var base_pt: Vector3 = box_clamp(aimv); var off: Vector3 = tgt - base_pt
					var lat := off.dot(net_right) * exp_side; var lon := off.dot(net_dir)
					var cdir := net_right * exp_side
					var blocked: bool = (box_clamp(base_pt + cdir * 0.1) - (base_pt + cdir * 0.1)).length() > 0.005 and off.length() < 0.02


					var fwd_max: float = float(sp.get("path_F", 0.0)) * float(sp.get("s_land", 0.0))
					ok = (lat > 0.05 and lon > -0.1 and lon < fwd_max + 0.15) or blocked
					detail += ", push %+.2f m to that side, %+.2f m along the hit%s" % [lat, lon, " [cursor at the court edge on that side]" if blocked else ""]
				elif ok and exp_shot == "float":
					var hit_something: bool = (Vector3(r["land"]) - Vector3(r["pred"].x, 0, r["pred"].z)).length() > 0.5
					var land2: Vector3 = Vector3(r["pred"].x, 0, r["pred"].z) if hit_something else Vector3(r["land"])
					var lat2 := (land2 - Vector3(r["aim"])).dot(net_right)
					var rng := (land2 - Vector3(r["contact"].x, 0, r["contact"].z)).length()
					var apex_rel: float = r["apex"] - r["contact"].y
					if exp_side == 0:
						ref_float = {"apex": apex_rel, "rng": rng}
						ok = absf(lat2) < 0.6; detail += ", lateral %+.2f m (straight), apex +%.2f m, range %.2f m" % [lat2, apex_rel, rng]
					else:
						ok = lat2 * exp_side > 0.5
						detail += ", lateral %+.2f m to that side, apex +%.2f m, range %.2f m" % [lat2, apex_rel, rng]
						if not ref_float.is_empty():
							ok = ok and absf(apex_rel - float(ref_float["apex"])) < 0.6 and absf(rng - float(ref_float["rng"])) < 0.25 * float(ref_float["rng"]) + 1.0
							detail += " (straight float: apex +%.2f, range %.2f)" % [float(ref_float["apex"]), float(ref_float["rng"])]
					if hit_something: detail += " [flight hit a post/wall: judged by the game's predicted landing]"
				verdict(name, ok, detail)
		cursor_world = Vector3.INF; InputCollector.dev_cursor = Vector2.INF
		for i in 8:
			if rig.mode == 1: break
			rig.call("cycle_mode"); await frames(5)

	if only.contains("spinbar"):
		walk_home = true
		for i in 8:
			if rig.mode == 1: break
			rig.call("cycle_mode"); await frames(5)
		mid_delay = int(OS.get_environment("JV_MID_DELAY")) if OS.get_environment("JV_MID_DELAY") != "" else 9
		var want_c := {"tap": [0.0, 0.3], "mid": [0.3, 0.8], "full": [0.8, 1.01]}
		for mode in ["tap", "mid", "full"]:
			press_mode = mode
			var got := {}
			for attempt in 5:
				cursor_world = Vector3(-1.5, 0, -4.5)
				shot_after_touch = "spinbar-" + mode
				var r: Dictionary = await one(["move_right"], "spinbar-" + mode)
				shot_after_touch = ""
				var ls: Dictionary = sh.get_last_spin()
				if r.is_empty() or not String(r.get("last", "")).begins_with("applied curveball"): continue
				got = r.duplicate(); got["spin"] = ls
				var ch: float = ls.get("charge", -1.0)
				p("spinbar %s attempt %d charge %.2f k %.2f A %.3f" % [mode, attempt, ch, float(ls.get("k", -1)), float(ls.get("ramp_A", -1))])
				if ch >= want_c[mode][0] and ch < want_c[mode][1]: break
				if mode == "mid":
					if ch >= 0.8: mid_delay += 3
					elif ch < 0.3: mid_delay = maxi(0, mid_delay - 3)
			if got.is_empty(): verdict("spinbar_" + mode, false, "no Curveball"); continue
			var ls2: Dictionary = got["spin"]
			var seen: Dictionary = bar_seen.get("spinbar-" + mode, {})
			var bar: Dictionary = seen.get("bar", {})
			var pts: Array = bar.get("points", [])
			var land: Vector3 = got["land"]
			var dmin := 99.0
			for i in range(1, pts.size()):
				var a: Vector3 = pts[i - 1]; var b2: Vector3 = pts[i]
				var ab := b2 - a; var t := clampf((land - a).dot(ab) / maxf(ab.length_squared(), 1e-9), 0.0, 1.0)
				dmin = minf(dmin, (land - (a + ab * t)).length())
			var tgt: Vector3 = ls2.get("target", Vector3.INF); var lnd: Vector3 = ls2.get("landing", Vector3.INF)
			var ch2: float = ls2.get("charge", -1.0)
			var kk: float = ls2.get("k", -1.0)

			var even: bool = ls2.get("even_bend", false)
			var k_want := 0.0 if even else 0.3 + 3.7 * clampf(ch2, 0, 1)
			var k_ok := absf(kk - k_want) < 0.01
			var start: Vector3 = ls2.get("path_start", Vector3.INF); var endp: Vector3 = ls2.get("path_end", Vector3.INF)
			var along := (tgt - start).length() / maxf((endp - start).length(), 0.01)
			verdict("spinbar_" + mode, bool(bar.get("on", false)) and int(bar.get("mesh_vertices", 0)) > 0 and dmin < float(seen.get("box", {}).get("spread", 0.35)) + 0.2 and (land - lnd).length() < 0.35 and k_ok,
				"charge %.2f -> k %.2f (expect %.2f, even %s, s %.2f); bar on=%s, %d mesh verts, from the cursor %s to the max-curve end %s; target %.0f %% along it; ball landed %.2f m from the bar's path, %.2f m from the solved landing; ramp A %.3f T %.2f; fade %.2f" % [ch2, kk, k_want, even, float(ls2.get("s_land", -1)), bar.get("on", false), int(bar.get("mesh_vertices", 0)), start, endp, along * 100.0, dmin, (land - lnd).length(), float(ls2.get("ramp_A", 0)), float(ls2.get("ramp_T", 0)), float(seen.get("box", {}).get("fade", -1))])

		press_mode = "full"
		cursor_world = Vector3(-1.5, 0, -4.5)
		shot_after_touch = "float-indicator"
		var fr: Dictionary = await one(["move_back"], "float-indicator")
		shot_after_touch = ""
		var fs: Dictionary = bar_seen.get("float-indicator", {})
		verdict("float_indicator", not fr.is_empty() and String(fr.get("last", "")).begins_with("applied float") and bool(fs.get("float_serve", false)), "Float box shown (float_serve %s, spread %.2f)" % [fs.get("float_serve", false), float(fs.get("spread", 0))])
		cursor_world = Vector3.INF; InputCollector.dev_cursor = Vector2.INF

	if only == "" or only.contains("cursor"):
		walk_home = false
		await get_tree().create_timer(2.0).timeout
		var targets := {"deep": Vector3(0, 0, -7.0), "short": Vector3(0, 0, -2.0), "left": Vector3(-3.0, 0, -4.5), "right": Vector3(3.0, 0, -4.5)}
		var spots := {"back": 7.0, "mid": 4.5, "net": 1.8}
		var boost_cases := 0
		for sp in spots:
			for tg in targets:
				var side := signf(pl.global_position.z) if absf(pl.global_position.z) > 0.3 else 1.0

				var spot := Vector3(pl.global_position.x * 0.5, 0, side * float(spots[sp]))
				await walk_to(spot); home_spot = spot
				var tw: Vector3 = targets[tg]; tw = Vector3(tw.x, 0, tw.z * side)
				var rn := {}
				for kind in [["normal", []], ["topspin", ["move_forward"]]]:
					cursor_world = tw
					rn[kind[0]] = await one(kind[1], "%s-%s-%s" % [sp, tg, kind[0]])
					if kind[0] == "topspin": rn["ts"] = sh.get_last_topspin()
					dump("cursor spot=%s target=%s case=%s" % [sp, tg, kind[0]], rn[kind[0]])
				cursor_world = Vector3.INF; InputCollector.dev_cursor = Vector2.INF
				var t: Dictionary = rn["topspin"]; var nm: Dictionary = rn["normal"]
				if t.is_empty(): verdict("cursor_%s_%s" % [sp, tg], false, "no topspin"); continue
				var err: float = (Vector3(t["land"]) - Vector3(t["aim"])).length()
				var perr: float = (Vector3(t["pred"].x, 0, t["pred"].z) - Vector3(t["land"])).length() if t["pred"] != Vector3.INF else 99.0
				var apx: float = t["apex"]; var napx: float = nm["apex"] if not nm.is_empty() else 0.0
				var net_top: float = float(ball.get("PHYS").get("net_height")) + float(ball.get("PHYS").get("radius"))
				var clear: float = t["net_y"] - net_top if t["net_y"] != INF else -99.0
				var ts: Dictionary = rn.get("ts", {})
				var same_n_speed: float = ts.get("normal_speed", 0.0); var same_n_apex: float = ts.get("normal_apex", 0.0)
				var faster: bool = float(ts.get("hspeed", 0.0)) > float(ts.get("normal_hspeed", 99.0)) + 0.01
				var higher: float = apx - same_n_apex
				verdict("cursor_%s_%s" % [sp, tg], String(t["last"]).begins_with("applied topspin") and err < 0.25 and perr < 0.05 and (t["net_y"] == INF or clear > 0.15) and faster and higher > -0.05 and higher < 1.0,
					"landing-cursor %.3f m, marker-landing %.3f m, net clearance %.2f m, speed %.1f vs the game's normal shot to the same cursor %.1f m/s (horizontal x%.2f), apex %.2f vs normal %.2f m (%+.2f), dip x%.2f | the game's own separate shot: speed %s apex %s" % [err, perr, clear, t["speed"], same_n_speed, float(ts.get("speed_k", 0)), apx, same_n_apex, higher, float(ts.get("dive_g", 0)), fmt(nm.get("speed", "-")), fmt(napx)])


				var bw: float = float(ts.get("spike_boost_wanted", 1.0)); var bu: float = float(ts.get("spike_boost", 1.0))
				var kp: float = float(ts.get("speed_k_plain", 0.0)); var kk2: float = float(ts.get("speed_k", 0.0))
				if bw > 1.0001: boost_cases += 1
				verdict("boost_%s_%s" % [sp, tg], ts.has("spike_boost") and (absf(bw - 1.0) < 1e-6 or absf(bw - 1.12) < 1e-6) and absf(kk2 - kp * bu) < 1e-6 and (bw <= 1.0001 or bu > 1.0001 or String(ts.get("spike_boost_note", "")).begins_with("skipped")),
					"spike boost wanted x%.3f used x%.3f (%s), k %.3f plain %.3f" % [bw, bu, ts.get("spike_boost_note", "?"), kk2, kp])
				await shot("cursor-%s-%s" % [sp, tg])
		verdict("spike_boost_on_real_spikes", boost_cases > 0, "%d of the air topspin touches were game spikes with the boost asked" % boost_cases)
	var failed := verdicts.filter(func(v): return not v[1]).map(func(v): return v[0])
	p("RESULT %s passed=%d failed=%s" % ["ALL PASS" if failed.is_empty() else "FAIL", verdicts.size() - failed.size(), str(failed)])
	p("done")
	get_tree().quit(0 if failed.is_empty() else 1)
