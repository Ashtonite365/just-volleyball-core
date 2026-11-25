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

func tl() -> Node:
	return get_node_or_null("/root/JVTextureLoader")

func goofy() -> Node:
	return get_node_or_null("/root/JVGoofyMods")

func report_ball(tag: String) -> void:
	if ball == null:
		p("%s no ball" % tag); return
	var phys = ball.get("PHYS")
	var pr: float = phys.radius if phys else -1.0
	var model = ball.get("_model")
	var vis := -1.0
	var names := []
	if model is Node3D and model.is_inside_tree():
		var inv: Transform3D = model.global_transform.affine_inverse()
		var box := AABB(); var first := true
		for mi in model.find_children("*", "MeshInstance3D", true, false):
			if mi.mesh == null: continue
			names.append("%s vis=%s meta=%s" % [mi.name, mi.visible, mi.has_meta("jvm_custom_ball")])
			if not mi.visible: continue
			var b: AABB = (inv * mi.global_transform) * mi.get_aabb()
			box = b if first else box.merge(b); first = false
		if not first:
			vis = box.get_longest_axis_size() * 0.5 * ball.global_basis.get_scale().x
	var customs := []
	for mi in ball.find_children("JVM_CustomBall", "MeshInstance3D", true, false):
		var pn = mi.get_parent().name if mi.get_parent() else "?"
		customs.append("%s parent=%s vis=%s scale=%.3f" % [mi.name, pn, mi.visible, mi.scale.x])
	var loader = tl()
	var st: Dictionary = loader.get_status() if loader else {}
	p("%s phys=%.3f visual_r=%.3f ball_scale=%.3f picked=%s custom=%s meshes=%s customs=%s" % [tag, pr, vis, ball.scale.x, st.get("picked_ball", ""), st.get("custom_ball", ""), names, customs])

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

func clear_cams(keep: Camera3D) -> void:
	if court_vp == null: return
	for c in court_vp.get_children():
		if c is Camera3D and c != keep:

			pass

func set_game_skin(id: String) -> void:
	if tl() and tl().has_method("set_picked_ball"):
		tl().set_picked_ball("")
	await frames(20)
	if ball and ball.has_method("set_ball_skin"):
		ball.set_ball_skin(id)
	await frames(20)

func set_custom(id: String) -> void:
	if tl() and tl().has_method("set_picked_ball"):
		var ok: bool = tl().set_picked_ball(id)
		p("set_picked_ball %s -> %s" % [id, ok])
	await frames(24)

func spin_mesh(deg: float) -> void:
	var mesh = ball.get("mesh")
	if mesh is Node3D:
		mesh.rotate_x(deg_to_rad(18.0))
		mesh.rotate_y(deg_to_rad(deg))

func set_big(v: float) -> void:
	var path := ProjectSettings.globalize_path("res://mods/goofy/00-default.json")
	var raw := FileAccess.get_file_as_string(path)
	var d = JSON.parse_string(raw)
	if typeof(d) != TYPE_DICTIONARY:
		d = {}
	d["big_ball"] = v
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string(JSON.stringify(d, "\t")); f.close()
	await get_tree().create_timer(1.5).timeout
	var g = goofy()
	if g and g.has_method("reload_config"):
		g.reload_config()
	p("big_ball set to %s, goofy=%s" % [v, g != null])

	await get_tree().create_timer(2.5).timeout

	if tl() and tl().has_method("apply_picked_cosmetics"):
		tl().apply_picked_cosmetics()
	await frames(30)


func galaxy_tex_dest() -> String:
	return ProjectSettings.globalize_path("res://mods/resources/packs/06_galaxy_ball/ball_tex.png")

func install_galaxy_tex(src: String) -> void:
	if src == "" or not FileAccess.file_exists(src):
		p("galaxy tex missing: %s" % src); return
	var bytes := FileAccess.get_file_as_bytes(src)
	var f := FileAccess.open(galaxy_tex_dest(), FileAccess.WRITE)
	if f == null:
		p("galaxy tex write failed"); return
	f.store_buffer(bytes); f.close()
	if tl() and tl().has_method("rescan_picker_packs"):
		tl().rescan_picker_packs()
	elif tl() and tl().has_method("apply_picked_cosmetics"):
		tl().apply_picked_cosmetics()
	p("galaxy tex <- %s (%s bytes)" % [src, bytes.size()])
	await frames(16)

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
		if i % 5 == 0:
			p("waiting scene=%s path=%s" % [m, m.scene_file_path if m else "?"])
		if i == 25:
			var gc = get_node_or_null("/root/GameConfig")
			if gc and gc.has_method("goto_scene"):
				p("fallback goto menu from %s" % (m.scene_file_path if m else "?"))
				gc.goto_scene("res://game/ui/menu.tscn")
	if m == null or not m.has_method("_try_freeplay"):
		p("no menu scene=%s path=%s" % [m, m.scene_file_path if m else "?"])
		get_tree().quit(3); return
	var sz := OS.get_environment("JV_PROBE_SIZE").split("x")
	var w := get_window(); w.mode = Window.MODE_WINDOWED; w.size = Vector2i(int(sz[0]), int(sz[1])); w.position = Vector2i.ZERO
	p("loaders tex=%s goofy=%s packs=%s" % [tl() != null, goofy() != null, tl().list_picker_packs("ball") if tl() else []])
	m.call("_try_freeplay")
	await get_tree().create_timer(5.0).timeout
	for n in get_tree().root.find_children("*", "Node", true, false):
		if n == m or n.get("camera_rig") == null: continue
		if n.get("player") is Node3D and n.get("ball") is Node3D and (n.get("player") as Node3D).is_visible_in_tree():
			host = n; break
	if host == null:
		p("no practice host"); get_tree().quit(3); return
	ball = host.get("ball")
	court_vp = ball.get_viewport()
	p("host=%s ball=%s vp=%s" % [host.get_path(), ball.get_path(), court_vp.get_path() if court_vp else "?"])
	if m.get("ui"):
		(m.get("ui") as CanvasItem).visible = false
	place_ball()
	await frames(10)


	for skin in [["school", "school"], ["pro", "nationals"], ["outdoor", "beach"]]:
		await set_game_skin(skin[1])
		place_ball()
		report_ball("game-" + skin[0])
		var c1 := cam_look(1.05, 0.12, 0.4)
		await shot("game-%s-close" % skin[0])
		c1.queue_free()
		var c2 := cam_look(2.8, 0.45, 0.55)
		await shot("game-%s-mid" % skin[0])
		c2.queue_free()


	var gal_before := OS.get_environment("JV_GALAXY_BEFORE")
	var gal_after := OS.get_environment("JV_GALAXY_AFTER")
	if gal_before != "":
		await install_galaxy_tex(gal_before)
		await set_custom("1006")
		place_ball()
		var mesh_b = ball.get("mesh")
		if mesh_b is Node3D:
			mesh_b.transform.basis = Basis()
			mesh_b.scale = Vector3.ONE
		report_ball("custom-galaxy-before-1.0")
		var cb1 := cam_look(1.05, 0.12, 0.4)
		await shot("galaxy-before-close")
		cb1.queue_free()
		var cb2 := cam_look(2.8, 0.45, 0.55)
		await shot("galaxy-before-mid")
		cb2.queue_free()
	if gal_after != "":
		await install_galaxy_tex(gal_after)


	for pack in [["galaxy", "1006"], ["fire", "1010"], ["beach", "1011"], ["tricolor", "1012"]]:
		await set_custom(pack[1])
		place_ball()

		var mesh = ball.get("mesh")
		if mesh is Node3D:
			mesh.transform.basis = Basis()
			mesh.scale = Vector3.ONE
		report_ball("custom-" + pack[0] + "-1.0")
		var c1 := cam_look(1.05, 0.12, 0.4)
		await shot("%s-close" % pack[0])
		c1.queue_free()
		var c2 := cam_look(2.8, 0.45, 0.55)
		await shot("%s-mid" % pack[0])
		c2.queue_free()
		spin_mesh(55.0)
		var c3 := cam_look(1.15, 0.18, 0.7)
		await shot("%s-spin" % pack[0])
		c3.queue_free()


	await set_big(2.0)
	place_ball(2.0)
	if gal_before != "":
		await install_galaxy_tex(gal_before)
		await set_custom("1006")
		place_ball(2.0)
		var mesh_b2 = ball.get("mesh")
		if mesh_b2 is Node3D:
			mesh_b2.transform.basis = Basis()
			mesh_b2.scale = Vector3.ONE
		report_ball("custom-galaxy-before-2.0")
		var cb3 := cam_look(1.8, 0.2, 0.4)
		await shot("galaxy-before-close-big2")
		cb3.queue_free()
		var cb4 := cam_look(4.2, 0.7, 0.55)
		await shot("galaxy-before-mid-big2")
		cb4.queue_free()
	if gal_after != "":
		await install_galaxy_tex(gal_after)

	for pack in [["galaxy", "1006"], ["fire", "1010"], ["beach", "1011"], ["tricolor", "1012"]]:
		await set_custom(pack[1])
		place_ball(2.0)
		report_ball("custom-" + pack[0] + "-2.0")
		var c1 := cam_look(1.8, 0.2, 0.4)
		await shot("%s-close-big2" % pack[0])
		c1.queue_free()
		var c2 := cam_look(4.2, 0.7, 0.55)
		await shot("%s-mid-big2" % pack[0])
		c2.queue_free()
		spin_mesh(55.0)
		var c3 := cam_look(2.0, 0.25, 0.7)
		await shot("%s-spin-big2" % pack[0])
		c3.queue_free()


	await set_game_skin("nationals")
	place_ball(2.0)
	report_ball("game-pro-2.0")
	var c := cam_look(1.8, 0.2, 0.4)
	await shot("game-pro-close-big2")
	c.queue_free()

	await set_big(1.0)
	p("done")
	get_tree().quit(0)
