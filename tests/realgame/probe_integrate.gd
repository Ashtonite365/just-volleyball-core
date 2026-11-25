extends Node


func p(msg: String) -> void: print("PROBE ", msg)
var out_dir := ""
var host: Node = null
var ball: Node3D = null
var court_vp: Viewport = null

func frames(n := 8) -> void:
	for i in n: await get_tree().process_frame
	await RenderingServer.frame_post_draw

func shot_vp(vp: Viewport, name: String) -> void:
	await frames(6)
	if vp == null: return
	var img := vp.get_texture().get_image()
	var path := out_dir.path_join(name + ".png")
	img.save_png(path)
	p("screenshot=%s size=%s" % [path, img.get_size()])

func shot(name: String) -> void:
	if court_vp:
		await shot_vp(court_vp, name)
	await shot_vp(get_viewport(), name + "-window")

func shot_window(name: String) -> void:
	await shot_vp(get_viewport(), name)

func tl() -> Node: return get_node_or_null("/root/JVTextureLoader")

func bar_btn(m: Node, prefix: String) -> Button:
	var bar = m.get("_bottom_bar")
	if bar == null: return null
	for c in bar.get_children():
		if c is Button and String(c.text).begins_with(prefix): return c
	return null

func tile_labels(grid: Node) -> PackedStringArray:
	var out: PackedStringArray = []
	if grid == null: return out
	for c in grid.get_children():
		var name := ""
		for l in c.find_children("*", "Label", true, false):
			if l.text != "": name = l.text; break
		if name == "" and c is Button: name = c.tooltip_text
		out.append(name)
	return out

func find_tile(grid: Node, want: String) -> Button:
	if grid == null: return null
	var w := want.to_lower()
	for c in grid.get_children():
		if not (c is Button): continue
		if String(c.get_meta("jvm_stage_pack", "")).to_lower() == w: return c
		if String(c.get_meta("jvm_ball_id", "")).to_lower() == w: return c
		if c.tooltip_text.to_lower() == w or c.tooltip_text.to_lower().begins_with(w): return c
		for l in c.find_children("*", "Label", true, false):
			if l.text.to_lower() == w or l.text.to_lower().begins_with(w): return c
	return null

func ball_grid() -> Node:
	var layer = get_tree().root.find_child("JvmBallPicker", true, false)
	if layer == null: return null
	var grids = layer.find_children("*", "GridContainer", true, false)
	return grids[0] if grids.size() else null

func place_ball(y := 1.35) -> void:
	if ball == null: return
	ball.visible = true
	if "frozen" in ball: ball.frozen = true
	var origin := Vector3(0, y, 0)
	if host and host.get("player") is Node3D:
		var pl: Node3D = host.get("player")
		origin = Vector3(pl.global_position.x, y, 0.0)
	ball.global_position = origin
	ball.velocity = Vector3.ZERO
	var mesh = ball.get("mesh")
	if mesh is Node3D:
		mesh.scale = Vector3.ONE
		mesh.position = Vector3.ZERO
		mesh.transform.basis = Basis()

func cam_look(dist: float, height := 0.15, yaw := 0.35) -> Camera3D:
	var vp: Viewport = court_vp if court_vp else ball.get_viewport()
	var cam := Camera3D.new()
	cam.fov = 40.0
	vp.add_child(cam)
	var t: Vector3 = ball.global_position
	var pos := t + Vector3(sin(yaw) * dist, height, cos(yaw) * dist)
	cam.look_at_from_position(pos, t, Vector3.UP)
	cam.current = true
	return cam

func report_ball(tag: String) -> void:
	if ball == null:
		p("%s no ball" % tag); return
	var st: Dictionary = tl().get_status() if tl() else {}
	var names := []
	var model = ball.get("_model")
	if model is Node:
		for mi in model.find_children("*", "MeshInstance3D", true, false):
			names.append("%s vis=%s meta=%s" % [mi.name, mi.visible, mi.has_meta("jvm_custom_ball")])
	p("%s picked=%s custom=%s meshes=%s" % [tag, st.get("picked_ball", ""), st.get("custom_ball", ""), names])

func grab_host() -> void:
	var m = get_tree().current_scene
	for n in get_tree().root.find_children("*", "Node", true, false):
		if n == m or n.get("camera_rig") == null: continue
		if n.get("player") is Node3D and n.get("ball") is Node3D and (n.get("player") as Node3D).is_visible_in_tree():
			host = n; break
	if host:
		ball = host.get("ball")
		court_vp = ball.get_viewport()
		p("host=%s ball=%s" % [host.get_path(), ball.get_path()])

func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
	var m: Node = null
	for i in 150:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"):
			p("boot -> continue"); m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			p("title -> menu"); get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_try_freeplay") and m.get("player") != null:
			await get_tree().create_timer(2.0).timeout
			break
	if m == null or not m.has_method("_try_freeplay"):
		p("no menu"); get_tree().quit(3); return
	var sz := OS.get_environment("JV_PROBE_SIZE").split("x")
	var w := get_window(); w.mode = Window.MODE_WINDOWED; w.size = Vector2i(int(sz[0]), int(sz[1])); w.position = Vector2i.ZERO
	var t := tl()
	p("loaders tex=%s packs_ball=%s packs_stage=%s" % [t != null, t.list_picker_packs("ball") if t else [], t.list_picker_packs("stage") if t else []])
	await shot_window("00-menu")


	var ballbtn := bar_btn(m, "ball:")
	p("ball_pill=%s" % [ballbtn.text if ballbtn else "?"])
	if ballbtn:
		ballbtn.pressed.emit(); await get_tree().create_timer(1.0).timeout
		var bg := ball_grid()
		p("ball_tiles=%s" % [str(tile_labels(bg))])
		await shot_window("01-ball-picker")
		var layer = get_tree().root.find_child("JvmBallPicker", true, false)
		if layer:
			for b in layer.find_children("*", "Button", true, false):
				if b.text.to_lower() == "done": b.pressed.emit(); break
		await get_tree().create_timer(0.6).timeout


	if t and t.has_method("set_picked_ball"):
		p("set galaxy -> %s" % t.set_picked_ball("1006"))
	m.call("_try_freeplay")
	await get_tree().create_timer(5.0).timeout
	grab_host()
	if host == null:
		p("no practice host"); get_tree().quit(3); return
	if m.get("ui"):
		(m.get("ui") as CanvasItem).visible = false
	place_ball()
	await frames(12)
	report_ball("galaxy")
	var c1 := cam_look(1.05, 0.12, 0.4)
	await shot("02-galaxy-close")
	c1.queue_free()
	var c2 := cam_look(2.8, 0.45, 0.55)
	await shot("03-galaxy-mid")
	c2.queue_free()


	if t: p("set fire -> %s" % t.set_picked_ball("1010"))
	await frames(20)
	place_ball()
	report_ball("fire")
	var c3 := cam_look(1.05, 0.12, 0.4)
	await shot("04-fire-close")
	c3.queue_free()


	if t: p("set pro (clear custom) -> %s" % t.set_picked_ball(""))
	await frames(8)
	if ball and ball.has_method("set_ball_skin"):
		ball.set_ball_skin("nationals")
	await frames(20)
	place_ball()
	report_ball("game-pro")
	var c4 := cam_look(1.05, 0.12, 0.4)
	await shot("05-game-pro-close")
	c4.queue_free()

	p("done")
	get_tree().quit(0)
