extends Node

func p(msg: String) -> void: print("PROBE ", msg)

func shot(tag: String) -> void:
	if OS.get_environment("JV_PROBE_OUT") == "": return
	for i in 6: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var path := OS.get_environment("JV_PROBE_OUT").path_join("%s.png" % tag)
	get_viewport().get_texture().get_image().save_png(path)
	p("screenshot=%s" % path)

func tl() -> Node: return get_node_or_null("/root/JVTextureLoader")

func bar_btn(m: Node, prefix: String) -> Button:
	var bar = m.get("_bottom_bar")
	if bar == null: return null
	for c in bar.get_children():
		if c is Button and String(c.text).begins_with(prefix): return c
	return null

func wait_menu() -> Node:
	var m: Node = null
	for i in 120:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"):
			p("boot scene %s -> continue" % m.scene_file_path); m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			p("title -> menu"); get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_card_btn") and m.find_child("JvmModsCard", true, false) != null:
			await get_tree().create_timer(1.0).timeout
			break
	return m

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

func stage_grid(m: Node) -> Node:
	var pk = m.get("_fp_picker")
	return pk.get("_grid") if pk else null

func ball_grid() -> Node:
	var pu = get_node_or_null("/root/JVModGate/JVMenuUI/JVPickerUI")
	if pu == null: return null

	var layer = get_tree().root.find_child("JvmBallPicker", true, false)
	if layer == null: return null
	var grids = layer.find_children("*", "GridContainer", true, false)
	return grids[0] if grids.size() else null

func _ready() -> void:
	var phase := OS.get_environment("JV_PROBE_PHASE")
	if phase == "": phase = "1"
	p("phase=%s" % phase)
	var m := await wait_menu()
	p("waited for menu")
	if m == null: p("FAIL no menu"); get_tree().quit(3); return
	var stagebtn := bar_btn(m, "stage:")
	var ballbtn := bar_btn(m, "ball:")
	p("pills stage='%s' ball='%s'" % [stagebtn.text if stagebtn else "?", ballbtn.text if ballbtn else "?"])
	var t := tl()
	p("loader=%s picked_stage=%s picked_ball=%s stage_packs=%s ball_packs=%s" % [
		t != null, t.get_picked_stage() if t else "?", t.get_picked_ball() if t else "?",
		t.list_picker_packs("stage") if t else [], t.list_picker_packs("ball") if t else []])

	if phase == "2":
		p("restart pills stage='%s' ball='%s'" % [stagebtn.text if stagebtn else "?", ballbtn.text if ballbtn else "?"])
		p("restart picked_stage=%s picked_ball=%s" % [t.get_picked_stage() if t else "?", t.get_picked_ball() if t else "?"])
		await shot("restart-menu")
		if m.has_method("_try_freeplay"):
			m.call("_try_freeplay"); await get_tree().create_timer(4.0).timeout
			await shot("restart-freeplay")
		p("done phase2")
		get_tree().quit(0)
		return


	if stagebtn == null: p("FAIL no stage pill"); get_tree().quit(4); return
	stagebtn.pressed.emit(); await get_tree().create_timer(1.0).timeout
	var pk = m.get("_fp_picker")
	p("stage_picker_open=%s" % (pk != null and pk.has_method("is_open") and pk.is_open()))
	var sg := stage_grid(m)
	var slabs := tile_labels(sg)
	p("stage_tiles=%s count=%d" % [str(slabs), slabs.size()])
	await shot("stage-picker")
	var royal := find_tile(sg, "Royal Court")
	p("royal_tile=%s last_tile=%s" % [royal != null, slabs[slabs.size()-1] if slabs.size() else ""])
	if royal:
		royal.pressed.emit(); await get_tree().create_timer(0.6).timeout
		await shot("stage-picker-royal")

	if pk:
		for b in pk.find_children("*", "Button", true, false):
			if b.text.to_lower() == "done": b.pressed.emit(); break
	await get_tree().create_timer(1.5).timeout
	p("after stage: pill='%s' picked=%s" % [stagebtn.text, t.get_picked_stage() if t else "?"])
	await shot("menu-royal-court")


	if ballbtn == null: p("FAIL no ball pill"); get_tree().quit(4); return
	ballbtn.pressed.emit(); await get_tree().create_timer(1.0).timeout
	var bg := ball_grid()
	var blabs := tile_labels(bg)
	p("ball_tiles=%s count=%d" % [str(blabs), blabs.size()])
	await shot("ball-picker")
	var galaxy := find_tile(bg, "Galaxy")
	p("galaxy_tile=%s" % (galaxy != null))
	if galaxy:
		galaxy.pressed.emit(); await get_tree().create_timer(0.6).timeout
		await shot("ball-picker-galaxy")

	var layer = get_tree().root.find_child("JvmBallPicker", true, false)
	if layer:
		for b in layer.find_children("*", "Button", true, false):
			if b.text.to_lower() == "done": b.pressed.emit(); break
	await get_tree().create_timer(1.0).timeout
	p("after ball: pill='%s' picked=%s" % [ballbtn.text, t.get_picked_ball() if t else "?"])
	await shot("menu-galaxy-ball")


	var drop := OS.get_environment("JV_DROP_PACK")
	if drop != "":
		var dest := OS.get_executable_path().get_base_dir().path_join("mods/resources/packs/07_sunrise_ball")
		DirAccess.make_dir_recursive_absolute(dest)
		var d := DirAccess.open(drop)
		if d:
			d.list_dir_begin()
			var fn := d.get_next()
			while fn != "":
				if not d.current_is_dir():
					d.copy(drop.path_join(fn), dest.path_join(fn))
				fn = d.get_next()
		p("dropped sunrise pack into %s" % dest)
		ballbtn.pressed.emit(); await get_tree().create_timer(1.0).timeout
		bg = ball_grid()
		blabs = tile_labels(bg)
		p("ball_tiles_after_drop=%s count=%d" % [str(blabs), blabs.size()])
		var sunrise := find_tile(bg, "Sunrise")
		p("sunrise_tile=%s" % (sunrise != null))
		await shot("ball-picker-sunrise-drop")
		if layer == null: layer = get_tree().root.find_child("JvmBallPicker", true, false)
		if layer:
			for b in layer.find_children("*", "Button", true, false):
				if b.text.to_lower() == "done": b.pressed.emit(); break
		await get_tree().create_timer(0.5).timeout


	ballbtn.pressed.emit(); await get_tree().create_timer(0.8).timeout
	bg = ball_grid()
	var pro := find_tile(bg, "pro")
	p("pro_tile=%s" % (pro != null))
	if pro: pro.pressed.emit(); await get_tree().create_timer(0.4).timeout
	layer = get_tree().root.find_child("JvmBallPicker", true, false)
	if layer:
		for b in layer.find_children("*", "Button", true, false):
			if b.text.to_lower() == "done": b.pressed.emit(); break
	await get_tree().create_timer(0.6).timeout
	p("after pro: pill='%s' picked_ball=%s" % [ballbtn.text, t.get_picked_ball() if t else "?"])

	ballbtn.pressed.emit(); await get_tree().create_timer(0.8).timeout
	bg = ball_grid()
	galaxy = find_tile(bg, "Galaxy")
	if galaxy: galaxy.pressed.emit(); await get_tree().create_timer(0.4).timeout
	layer = get_tree().root.find_child("JvmBallPicker", true, false)
	if layer:
		for b in layer.find_children("*", "Button", true, false):
			if b.text.to_lower() == "done": b.pressed.emit(); break
	await get_tree().create_timer(0.6).timeout
	p("reselected galaxy: pill='%s' picked=%s" % [ballbtn.text, t.get_picked_ball() if t else "?"])

	if m.has_method("_try_freeplay"):
		p("freeplay")
		m.call("_try_freeplay")
		await get_tree().create_timer(4.0).timeout
		await shot("freeplay-royal-galaxy")
	p("done phase1")
	get_tree().quit(0)
