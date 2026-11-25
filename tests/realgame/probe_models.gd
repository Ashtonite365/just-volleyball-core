extends Node





func p(msg: String) -> void: print("PROBE ", msg)
var m: Node = null
var out_dir := ""
func shot(name: String) -> void:
	for i in 6: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	var path := out_dir.path_join(name + ".png")
	img.save_png(path); p("screenshot=%s size=%s" % [path, img.get_size()])

func grafts(rig: Node) -> Array:
	var out := []
	var sk: Node = rig.get("_skel")
	if sk: for c in sk.get_children(): if c.has_meta("jvm_model"): out.append(c)
	return out

func visible_base(rig: Node) -> int:
	var n := 0
	for g in rig.find_children("*", "GeometryInstance3D", true, false):
		if not g.has_meta("jvm_model") and (g as Node3D).visible: n += 1
	return n

func pose(pl: Node, rig: Node, clip: String, t: float) -> String:
	var anim: AnimationPlayer = rig.get("_anim")
	var found: String = rig.call("play_clip", clip, false)
	if found == "": return ""
	anim.speed_scale = 1.0
	anim.play(found, 0.0)
	anim.seek(t * anim.get_animation(found).length, true)
	anim.speed_scale = 0.0
	(rig.get("_skel") as Skeleton3D).force_update_all_bone_transforms()

	var hl: Node3D = pl.get("hand_l"); var hr: Node3D = pl.get("hand_r")
	if hl and hr and rig.call("hands_animated"):
		hl.position = rig.call("hand_local", false); hr.position = rig.call("hand_local", true)
	return found

func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
	for i in 150:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"):
			p("boot scene -> continue"); m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			p("title -> menu"); get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_card_btn") and m.get("player") != null:
			await get_tree().create_timer(2.0).timeout
			break
	if m == null or m.get("player") == null: p("no menu player"); get_tree().quit(3); return
	var sz := OS.get_environment("JV_PROBE_SIZE").split("x")
	var w := get_window(); w.mode = Window.MODE_WINDOWED; w.size = Vector2i(int(sz[0]), int(sz[1])); w.position = Vector2i.ZERO
	var ml: Node = get_node_or_null("/root/JVModelLoader")
	p("loader=%s status=%s" % [ml != null, ml.get_status() if ml else {}])
	var pl: Node3D = m.get("player")
	var rig: Node = pl.get("_model")
	p("menu player=%s rig=%s class=%s" % [pl.get_path(), rig.get_path() if rig else "null", rig.get_script().get_global_name() if rig else "?"])
	var van := {"hand_radius": rig.get("hand_radius"), "hand_rest": rig.get("hand_rest"), "scale": rig.get("_base_scale")}
	p("vanilla_rig hand_radius=%.5f hand_rest=%s base_scale=%.5f" % [van.hand_radius, van.hand_rest, van.scale])

	var cam := Camera3D.new(); cam.fov = 40.0; m.add_child(cam)
	var ui = m.get("ui")
	var root := ProjectSettings.globalize_path("res://mods/models")
	var cases := ("" if OS.get_environment("JV_MODEL_CASES") == "" else OS.get_environment("JV_MODEL_CASES")).split(",")
	cases.insert(0, ":vanilla")


	pl.set_process(false); pl.set_physics_process(false); m.set_process(false); m.set_physics_process(false)
	var ball = m.get("ball")
	for c in cases:
		var id: String = c.split(":")[0]; var label: String = c.split(":")[1]
		var f := FileAccess.open(root.path_join("equipped.json"), FileAccess.WRITE)
		f.store_string(JSON.stringify({"equipped": [], "model": id})); f.close()
		ml.call("reload_config")
		for i in 10: await get_tree().process_frame
		var g := grafts(rig)
		var st: Dictionary = ml.get_status()
		p("case=%s id=%s grafts=%d visible_base_geoms=%d model=%s applied=%s last_error=%s" % [label, id, g.size(), visible_base(rig), st.get("model"), st.get("applied"), st.get("last_error", "")])
		p("case=%s rig_report=%s" % [label, str(ml.get_rig_report())])
		if g.size():
			var mi := g[0] as MeshInstance3D
			var named := 0
			for b in mi.skin.get_bind_count(): named += 1 if (rig.get("_skel") as Skeleton3D).find_bone(mi.skin.get_bind_name(b)) >= 0 else 0
			var mat: Material = mi.material_override if mi.material_override else mi.get_surface_override_material(0)
			p("case=%s graft=%s parent=%s skeleton=%s binds=%d resolved=%d mat=%s shader=%s" % [label, mi.name, mi.get_parent().name, mi.skeleton, mi.skin.get_bind_count(), named, mat.get_class() if mat else "null",
				(mat as ShaderMaterial).shader.resource_path if mat is ShaderMaterial and (mat as ShaderMaterial).shader else "-"])
			var jersey: Material = (rig.get("_mats") as Dictionary).get("jersey")
			var tint = (mat as ShaderMaterial).get_shader_parameter("albedo") if mat is ShaderMaterial else (mat as StandardMaterial3D).albedo_color
			p("case=%s jersey_albedo=%s model_albedo=%s" % [label, jersey.get_shader_parameter("albedo") if jersey else "?", tint])
		p("case=%s hand_radius=%.5f hand_rest=%s base_scale=%.5f hands_animated=%s" % [label, rig.get("hand_radius"), rig.get("hand_rest"), rig.get("_base_scale"), rig.call("hands_animated")])

		cam.current = false
		pose(pl, rig, "idle", 0.3)
		await shot("%s-menu_view" % label)

		if ui: ui.visible = false
		if ball: ball.visible = false
		var fwd := -pl.global_transform.basis.z.normalized()
		var side := pl.global_transform.basis.x.normalized()
		var target := pl.global_position + Vector3(0, 0.95, 0)
		cam.global_position = target + side * 3.6 - fwd * 1.2 + Vector3(0, 0.35, 0)
		cam.look_at(target); cam.current = true
		for clip in [["idle", 0.3], ["spike", 0.45], ["recieve", 0.5], ["dive", 0.5], ["set", 0.5]]:
			var found := pose(pl, rig, clip[0], clip[1])
			p("case=%s pose=%s clip=%s t=%.2f hand_l=%s hand_r=%s" % [label, clip[0], found, clip[1], (pl.get("hand_l") as Node3D).position, (pl.get("hand_r") as Node3D).position])
			await shot("%s-%s" % [label, clip[0]])

		var jm: ShaderMaterial = (rig.get("_mats") as Dictionary).get("jersey")
		var orig: Color = jm.get_shader_parameter("albedo") if jm else Color.WHITE
		pose(pl, rig, "idle", 0.3)
		rig.call("set_slot_color", "jersey", Color("#2f6bff"))
		for i in 4: await get_tree().process_frame
		if g.size():
			var mi2 := g[0] as MeshInstance3D
			var mat2: Material = mi2.material_override if mi2.material_override else mi2.get_surface_override_material(0)
			p("case=%s team_blue model_albedo=%s" % [label, (mat2 as ShaderMaterial).get_shader_parameter("albedo") if mat2 is ShaderMaterial else (mat2 as StandardMaterial3D).albedo_color])
		await shot("%s-team_blue" % label)
		rig.call("set_slot_color", "jersey", orig)
		if ui: ui.visible = true
		if ball: ball.visible = true

	var f2 := FileAccess.open(root.path_join("equipped.json"), FileAccess.WRITE)
	f2.store_string(JSON.stringify({"equipped": [], "model": cases[cases.size() - 1].split(":")[0]})); f2.close()
	ml.call("reload_config")
	var rig2: Node = load("res://game/character/model_rig.gd").new()
	pl.get_node("Visual").add_child(rig2)
	rig2.set("stencil_id", 2)
	rig2.call("setup")
	for i in 5: await get_tree().process_frame
	p("new_rig hand_radius=%.5f hand_rest=%s base_scale=%.5f grafts=%d same_as_vanilla=%s" % [rig2.get("hand_radius"), rig2.get("hand_rest"), rig2.get("_base_scale"), grafts(rig2).size(),
		is_equal_approx(rig2.get("hand_radius"), van.hand_radius) and (rig2.get("hand_rest") as Vector3).is_equal_approx(van.hand_rest) and is_equal_approx(rig2.get("_base_scale"), van.scale)])
	rig2.queue_free()
	p("done")
	get_tree().quit(0)
