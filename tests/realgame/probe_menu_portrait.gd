extends Node

func p(msg: String) -> void:
	print("PROBE ", msg)

var m: Node = null
var out_dir := ""

func shot(name: String) -> void:
	for i in 6:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	var path := out_dir.path_join(name + ".png")
	img.save_png(path)
	p("screenshot=%s size=%s" % [path, img.get_size()])

func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
	for i in 150:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null:
			continue
		if m.has_method("_on_continue") and not m.get("_left"):
			p("boot scene -> continue")
			m.call("_on_continue")
			continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			p("title -> menu")
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn")
			m.set("_leaving", true)
			continue
		if m.has_method("_card_btn") and m.get("player") != null:
			p("menu ready")
			break
	if m == null or m.get("player") == null:
		p("no menu player")
		get_tree().quit(3)
		return
	var sz := OS.get_environment("JV_PROBE_SIZE").split("x")
	var w := get_window()
	w.mode = Window.MODE_WINDOWED
	w.size = Vector2i(int(sz[0]), int(sz[1]))
	w.position = Vector2i.ZERO
	var ml: Node = get_node_or_null("/root/JVModelLoader")
	var tl: Node = get_node_or_null("/root/JVTextureLoader")
	if tl == null:
		var loader := get_node_or_null("/root/JVLoader")
		if loader and loader.has_method("model_node"):
			ml = loader.call("model_node")
			tl = loader.call("texture_node")
	p("model_loader=%s texture_loader=%s" % [ml != null, tl != null])
	if ml and ml.has_method("get_status"):
		p("models_status=%s" % [ml.get_status()])
	await get_tree().create_timer(4.0).timeout
	var portraits := 0
	var portrait_grafts := 0
	var root := get_tree().root
	for i in root.get_child_count():
		var n: Node = root.get_child(i)
		if not (n is SubViewport):
			continue
		var vp := n as SubViewport
		if not vp.own_world_3d or not vp.transparent_bg:
			continue
		for j in n.get_child_count():
			var c: Node = n.get_child(j)
			if String(c.get_class()) == "" :
				pass
			var script_ok := c.get_script() != null and String(c.get_script().get_global_name()) == "Player"
			if not script_ok:
				continue
			if c.process_mode != Node.PROCESS_MODE_DISABLED:
				continue
			portraits += 1
			var grafts := 0
			for g in c.find_children("*", "GeometryInstance3D", true, false):
				if g.has_meta("jvm_model"):
					grafts += 1
			portrait_grafts += grafts
			p("portrait player=%s process=%s grafts=%d" % [c.get_path(), c.process_mode, grafts])
	p("portrait_players=%d portrait_grafts=%d" % [portraits, portrait_grafts])
	var pfp_ok := false
	var av: TextureRect = m.get("_pc_avatar")
	if av and av.texture:
		var teximg: Image = av.texture.get_image() if av.texture.has_method("get_image") else null
		if teximg:
			var pfp_path := out_dir.path_join("menu-pfp.png")
			teximg.save_png(pfp_path)
			var used := teximg.get_used_rect()
			var r := 0.0
			var g := 0.0
			var b := 0.0
			var n := 0
			for y in range(used.position.y, used.end.y):
				for x in range(used.position.x, used.end.x):
					var px: Color = teximg.get_pixel(x, y)
					if px.a < 0.2:
						continue
					r += px.r
					g += px.g
					b += px.b
					n += 1
			if n > 0:
				r /= n
				g /= n
				b /= n
			pfp_ok = r > g + 0.08
			p("pfp path=%s size=%s used=%s pixels=%d mean_rgb=%.3f,%.3f,%.3f redder_than_green=%s" % [pfp_path, teximg.get_size(), used, n, r, g, b, str(pfp_ok)])
		else:
			p("pfp texture has no image")
	else:
		p("no _pc_avatar texture")
	var pl: Node = m.get("player")
	var menu_grafts := 0
	if pl:
		var rig: Node = pl.get("_model")
		if rig:
			var sk: Node = rig.get("_skel")
			if sk:
				for ch in sk.get_children():
					if ch.has_meta("jvm_model"):
						menu_grafts += 1
	p("menu_player_grafts=%d" % menu_grafts)
	await shot("menu-after-cosmetics")
	var ok := menu_grafts > 0 and (portrait_grafts > 0 or pfp_ok)
	p("verdict menu_portrait_cosmetics=%s pfp_ok=%s portrait_grafts=%d menu_grafts=%d" % ["PASS" if ok else "FAIL", pfp_ok, portrait_grafts, menu_grafts])
	get_tree().quit(0 if ok else 4)
