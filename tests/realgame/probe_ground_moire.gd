extends Node


func p(msg: String) -> void: print("PROBE ", msg)

var out_dir := ""

func shot(name: String) -> Image:
	for i in 8:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	var path := out_dir.path_join(name + ".png")
	img.save_png(path)
	p("screenshot=%s size=%s" % [path, img.get_size()])
	return img

func tex_info(tex: Texture2D) -> String:
	if tex == null:
		return "null"
	var img := tex.get_image()
	if img == null:
		return "id=%s size=%dx%d no-image" % [tex.get_instance_id(), tex.get_width(), tex.get_height()]
	return "id=%s size=%dx%d format=%s has_mipmaps=%s mipmap_count=%s bytes=%s" % [
		tex.get_instance_id(), tex.get_width(), tex.get_height(),
		img.get_format(), img.has_mipmaps(), img.get_mipmap_count(), img.get_data().size()
	]

func walk_mats(n: Node, budget: int, into: Array) -> void:
	if budget <= 0 or n == null:
		return
	if n is MeshInstance3D:
		var mi := n as MeshInstance3D
		var mats: Array = []
		if mi.material_override:
			mats.append(mi.material_override)
		for s in mi.get_surface_override_material_count():
			var ov := mi.get_surface_override_material(s)
			if ov:
				mats.append(ov)
			elif mi.mesh:
				var sm := mi.mesh.surface_get_material(s) if s < mi.mesh.get_surface_count() else null
				if sm:
					mats.append(sm)
		for mat in mats:
			into.append([str(mi.get_path()), mi.name, mat])
	for c in n.get_children():
		walk_mats(c, budget - 1, into)

func inspect(tag: String) -> void:
	var vram := 0
	if RenderingServer.has_method("get_rendering_info"):
		vram = int(RenderingServer.get_rendering_info(RenderingServer.RENDERING_INFO_TEXTURE_MEM_USED))
	p("vram_texture_mem=%s tag=%s" % [vram, tag])
	var loader := get_node_or_null("/root/JVTextureLoader")
	if loader and loader.has_method("get_mip_stats"):
		p("mip_stats=%s tag=%s" % [loader.get_mip_stats(), tag])
		p("status=%s tag=%s" % [loader.get_status(), tag])
	elif loader:
		p("mip_stats=unavailable (old build) tag=%s status=%s" % [tag, loader.get_status() if loader.has_method("get_status") else {}])
	else:
		p("mip_stats=no-loader tag=%s" % tag)
	var hits: Array = []
	walk_mats(get_tree().root, 24, hits)
	for h in hits:
		var path: String = h[0]
		var nm: String = str(h[1])
		if nm != "CourtFloor" and nm != "EnvFloor" and not path.contains("CourtFloor") and not path.contains("EnvFloor") and not path.contains("Grass"):
			continue
		var mat: Material = h[2]
		var bits := []
		if mat is BaseMaterial3D:
			var bm := mat as BaseMaterial3D
			bits.append("filter=%s aniso=%s albedo=%s" % [bm.texture_filter, bm.anisotropy, tex_info(bm.albedo_texture)])
		elif mat is ShaderMaterial:
			var sm := mat as ShaderMaterial
			for key in ["albedo_tex", "tex", "grass_tex", "sand_tex", "floor_tex"]:
				var v = sm.get_shader_parameter(key)
				if v is Texture2D:
					bits.append("%s=%s" % [key, tex_info(v)])
		if bits.size():
			p("mat node=%s name=%s %s tag=%s" % [path, nm, " ".join(bits), tag])

func aim_menu_cam() -> void:
	var scene := get_tree().current_scene
	if scene == null:
		return
	var cam: Camera3D = scene.get_node_or_null("MenuCam") as Camera3D
	if cam == null:
		cam = scene.find_child("MenuCam", true, false) as Camera3D
	if cam == null:
		p("no MenuCam")
		return
	cam.current = true

	cam.global_position = Vector3(0.0, 7.5, 16.0)
	cam.look_at(Vector3(0.0, 0.0, 0.0), Vector3.UP)
	p("cam pos=%s rot=%s" % [cam.global_position, cam.global_rotation_degrees])

func frame_sample(tag: String, n: int = 45) -> void:
	var times: Array[float] = []
	for i in n:
		var a := Time.get_ticks_usec()
		await get_tree().process_frame
		times.append((Time.get_ticks_usec() - a) / 1000.0)
	times.sort()
	var sum := 0.0
	for t in times:
		sum += t
	p("frames tag=%s n=%d avg_ms=%.3f med_ms=%.3f max_ms=%.3f" % [
		tag, times.size(), sum / times.size(), times[times.size() / 2], times[times.size() - 1]
	])

func wait_menu() -> Node:
	var m: Node = null
	for i in 150:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null:
			continue
		if m.has_method("_on_continue") and not m.get("_left"):
			p("boot scene -> continue")
			m.call("_on_continue")
			continue

		for b in get_tree().root.find_children("*", "Button", true, false):
			var txt := str(b.text).to_lower().replace("’", "'").replace("‘", "'")
			if txt.find("need to") >= 0:
				p("boot settings prompt -> skip text=%s" % b.text)
				b.pressed.emit()
				break

		if m.scene_file_path == "res://game/character/locker.tscn":
			p("onboarding locker -> menu")
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn")
			continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			p("title -> menu")
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn")
			m.set("_leaving", true)
			continue
		if m.has_method("_card_btn") and m.get("player") != null:
			await get_tree().create_timer(2.0).timeout
			return m
	return m

func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
	var mode := OS.get_environment("JV_MOIRE_MODE")
	if mode == "":
		mode = "stock"
	var tag := OS.get_environment("JV_MOIRE_TAG")
	if tag == "":
		tag = mode
	p("start mode=%s tag=%s" % [mode, tag])
	var m := await wait_menu()
	if m == null:
		p("no menu")
		get_tree().quit(3)
		return
	var w := get_window()
	w.mode = Window.MODE_WINDOWED
	var sz := OS.get_environment("JV_PROBE_SIZE").split("x")
	if sz.size() >= 2:
		w.size = Vector2i(int(sz[0]), int(sz[1]))
	w.position = Vector2i.ZERO
	var loader := get_node_or_null("/root/JVTextureLoader")
	if mode == "royal" and loader and loader.has_method("set_picked_stage"):
		var ok = loader.set_picked_stage("1005")
		p("pick_royal=%s" % ok)
		if loader.has_method("apply_picked_cosmetics"):
			loader.apply_picked_cosmetics()
		await get_tree().create_timer(1.0).timeout
	aim_menu_cam()
	await get_tree().create_timer(1.0).timeout
	inspect("menu-" + tag)
	await frame_sample("menu-" + tag)
	await shot("menu-ground-" + tag)

	get_node("/root/GameConfig").goto_scene("res://game/character/locker.tscn")
	var lk: Node = null
	for i in 40:
		await get_tree().create_timer(0.5).timeout
		lk = get_tree().current_scene
		if lk and lk.scene_file_path == "res://game/character/locker.tscn":
			break
	if lk and lk.scene_file_path == "res://game/character/locker.tscn":
		await get_tree().create_timer(1.5).timeout
		inspect("locker-" + tag)
		await shot("locker-ground-" + tag)
		get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn")
		await get_tree().create_timer(2.0).timeout
		aim_menu_cam()
		await get_tree().create_timer(0.8).timeout
		await shot("menu-after-locker-" + tag)
	else:
		p("locker not opened")
	p("done tag=%s" % tag)
	get_tree().quit(0)
