extends Node






func p(msg: String) -> void: print("PROBE ", msg)
var out_dir := ""
func shot(name: String) -> void:
	for i in 8: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	var path := out_dir.path_join(name + ".png")
	img.save_png(path); p("screenshot=%s size=%s" % [path, img.get_size()])

func grafts(rig: Node) -> Array:
	var out := []
	var sk: Node = rig.get("_skel") if rig else null
	if sk: for c in sk.get_children(): if c.has_meta("jvm_model"): out.append(c)
	return out

func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
	var m: Node = null
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
	if m == null: p("no menu"); get_tree().quit(3); return
	var sz := OS.get_environment("JV_PROBE_SIZE").split("x")
	var w := get_window(); w.mode = Window.MODE_WINDOWED; w.size = Vector2i(int(sz[0]), int(sz[1])); w.position = Vector2i.ZERO
	var ml: Node = get_node_or_null("/root/JVModelLoader")
	var root := ProjectSettings.globalize_path("res://mods/models")
	var models := [OS.get_environment("JV_LOCKER_MODEL")]
	if OS.get_environment("JV_LOCKER_MODEL2") != "": models.append(OS.get_environment("JV_LOCKER_MODEL2"))
	for mi in models.size():
		var spec: String = models[mi]
		var id: String = spec.split(":")[0]; var label: String = spec.split(":")[1]
		var f := FileAccess.open(root.path_join("equipped.json"), FileAccess.WRITE)
		f.store_string(JSON.stringify({"equipped": [], "model": id})); f.close()
		ml.call("reload_config")
		get_node("/root/GameConfig").goto_scene("res://game/character/locker.tscn")
		var lk: Node = null
		for i in 40:
			await get_tree().create_timer(0.5).timeout
			lk = get_tree().current_scene
			if lk and lk.scene_file_path == "res://game/character/locker.tscn" and (lk.get("_rail_btns") as Dictionary).has("jvm_full_model"): break
		if lk == null or lk.scene_file_path != "res://game/character/locker.tscn": p("locker not opened"); get_tree().quit(4); return
		var rail: Dictionary = lk.get("_rail_btns")
		p("locker=%s rail=%s gate=%s" % [lk.get_path(), str(rail.keys()), str(ml.get_status().get("reason"))])
		if not rail.has("jvm_full_model"):
			p("no tab; log tail follows"); get_tree().quit(5); return
		await get_tree().create_timer(1.0).timeout
		(rail["jvm_full_model"] as Button).pressed.emit()
		await get_tree().create_timer(1.5).timeout
		var pl: Node = lk.get("_player")
		var rig: Node = pl.get("_model") if pl else null
		var g := grafts(rig)
		p("case=%s section=%s preview_grafts=%d shown=%s saved=%s settings=%s" % [label, lk.get("_section"), g.size(), ml.get_shown_model(), ml.get_saved_model(), str(ml.get_model_settings(id))])
		var mat: Material = null
		if g.size():
			var gm := g[0] as MeshInstance3D
			mat = gm.material_override if gm.material_override else gm.get_surface_override_material(0)
		var colour = (mat as ShaderMaterial).get_shader_parameter("albedo") if mat is ShaderMaterial else ((mat as StandardMaterial3D).albedo_color if mat else null)
		p("case=%s model_albedo=%s" % [label, colour])
		await shot("locker-full_model-%s" % label)

		var content: Node = lk.get("_content")
		var box: Node = content.get_node_or_null("JvmFullModel")
		var sw: Button = null
		if box: for b in box.find_children("*", "Button", true, false): if b.has_meta("jvm_color") and (b.get_meta("jvm_color") as Color).to_html(false) == "2f6bff": sw = b
		p("case=%s swatch=%s" % [label, sw != null])
		if sw: sw.pressed.emit()
		await get_tree().create_timer(1.0).timeout
		g = grafts(rig)
		if g.size():
			var gm2 := g[0] as MeshInstance3D
			mat = gm2.material_override if gm2.material_override else gm2.get_surface_override_material(0)
		colour = (mat as ShaderMaterial).get_shader_parameter("albedo") if mat is ShaderMaterial else ((mat as StandardMaterial3D).albedo_color if mat else null)
		p("case=%s after_swatch model_albedo=%s settings=%s" % [label, colour, str(ml.get_model_settings(id))])
		await shot("locker-full_model-%s-colour" % label)
		ml.call("reset_model_color", id); ml.call("flush_settings")
		get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn")
		await get_tree().create_timer(3.0).timeout
	p("done")
	get_tree().quit(0)
