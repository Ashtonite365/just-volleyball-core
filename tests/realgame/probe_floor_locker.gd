extends Node


func p(msg: String) -> void:
	print("PROBE ", msg)

var out_dir := ""

func wait_frames(n: int = 10) -> void:
	for i in n:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw

func save_img(img: Image, name: String) -> void:
	if img == null or out_dir == "":
		p("skip shot %s (no image or out_dir)" % name)
		return
	var path := out_dir.path_join(name + ".png")
	img.save_png(path)
	p("screenshot=%s size=%s" % [path, img.get_size()])

func shot_window(name: String) -> void:
	await wait_frames(12)
	var vp := get_viewport()
	if vp and vp.get_texture():
		save_img(vp.get_texture().get_image(), name + "-window")


func _nvis(n: Node) -> String:
	if n is CanvasItem:
		return "v=%s vit=%s" % [(n as CanvasItem).visible, (n as CanvasItem).is_visible_in_tree()]
	if n is Node3D:
		return "v=%s vit=%s" % [(n as Node3D).visible, (n as Node3D).is_visible_in_tree()]
	return ""

func _nlayer(n: Node) -> String:
	if n is VisualInstance3D:
		return " layers=%s" % (n as VisualInstance3D).layers
	return ""

func dump_walk(f: FileAccess, n: Node, depth: int) -> void:
	if n == null or depth > 24:
		return
	var extra := ""
	if n is Camera3D:
		var cam := n as Camera3D
		extra = " Camera current=%s cull=%s fov=%s" % [cam.current, cam.cull_mask, cam.fov]
	elif n is SubViewport:
		var sv := n as SubViewport
		var c = sv.get_camera_3d()
		extra = " SubViewport size=%s update=%s cam=%s" % [sv.size, sv.render_target_update_mode, str(c.get_path()) if c else "none"]
	elif n is MeshInstance3D:
		var mi := n as MeshInstance3D
		var mat = mi.material_override
		var mat_s := ""
		if mat:
			mat_s = " mat=%s" % mat.get_class()
			if mat is BaseMaterial3D:
				mat_s += " albedo=%s" % (mat as BaseMaterial3D).albedo_color
			elif mat is ShaderMaterial:
				var sm := mat as ShaderMaterial
				var sh = sm.shader
				mat_s += " shader=%s" % (sh.resource_path if sh else "?")
				if sm.get_shader_parameter("albedo") != null:
					mat_s += " albedo_p=%s" % sm.get_shader_parameter("albedo")
		var mesh_n := ""
		if mi.mesh:
			mesh_n = " mesh=%s" % mi.mesh.get_class()
		extra = " Mesh%s%s%s" % [mesh_n, mat_s, " jvm_model" if mi.has_meta("jvm_model") else ""]
	var meta := ""
	if n.has_meta("jvm_model"):
		meta = " META_jvm_model"
	if n.has_meta("jvm_accessory") or n.has_meta("jvm_acc_id"):
		meta += " META_acc"
	f.store_line("%s%s [%s] %s%s%s%s" % [
		"  ".repeat(depth), n.name, n.get_class(), str(n.get_path()), _nvis(n), _nlayer(n), extra + meta
	])
	for i in n.get_child_count():
		dump_walk(f, n.get_child(i), depth + 1)

func dump_locker(lk: Node, tag: String) -> void:
	if lk == null or out_dir == "":
		p("skip dump %s" % tag)
		return
	var path := out_dir.path_join("tree-locker-%s.txt" % tag)
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		p("FAIL dump open %s" % path)
		return
	f.store_line("=== locker dump %s scene=%s ===" % [tag, lk.scene_file_path if lk else "?"])
	var pl = lk.get("_player")
	f.store_line("_player=%s" % (str(pl.get_path()) if pl is Node else str(pl)))
	var world = lk.get("world")
	f.store_line("world=%s" % (str(world.get_path()) if world is Node else str(world)))
	var section = lk.get("_section")
	f.store_line("section=%s studio=%s" % [section, lk.get("_studio")])
	var mlstat = get_node_or_null("/root/JVModelLoader")
	if mlstat:
		f.store_line("shown=%s saved=%s previewing=%s grafts=%s" % [
			mlstat.get_shown_model() if mlstat.has_method("get_shown_model") else "?",
			mlstat.get_saved_model() if mlstat.has_method("get_saved_model") else "?",
			mlstat.is_previewing() if mlstat.has_method("is_previewing") else "?",
			mlstat.get_graft_count() if mlstat.has_method("get_graft_count") else "?"
		])
	var vps = lk.find_children("*", "SubViewport", true, false)
	f.store_line("=== SubViewports (%s) ===" % vps.size())
	for item in vps:
		var sv := item as SubViewport
		if sv == null:
			continue
		var cam = sv.get_camera_3d()
		f.store_line("SV %s size=%s update=%s world=%s cam=%s cull=%s current=%s" % [
			str(sv.get_path()), sv.size, sv.render_target_update_mode,
			"same" if sv.world_3d else "null-or-own",
			str(cam.get_path()) if cam else "none",
			cam.cull_mask if cam else 0,
			cam.current if cam else false
		])
	var cams = lk.find_children("*", "Camera3D", true, false)
	f.store_line("=== Camera3D (%s) ===" % cams.size())
	for item in cams:
		var cam := item as Camera3D
		if cam == null:
			continue
		f.store_line("CAM %s current=%s cull=%s fov=%s %s" % [
			str(cam.get_path()), cam.current, cam.cull_mask, cam.fov, _nvis(cam)
		])
	f.store_line("=== visible MeshInstance3D ===")
	var meshes = lk.find_children("*", "MeshInstance3D", true, false)
	var vis_n := 0
	for item in meshes:
		var mi := item as MeshInstance3D
		if mi == null or not mi.is_visible_in_tree():
			continue
		vis_n += 1
		var mat = mi.material_override
		var mat_s := ""
		if mat is BaseMaterial3D:
			mat_s = " albedo=%s" % (mat as BaseMaterial3D).albedo_color
		elif mat is ShaderMaterial:
			var sm := mat as ShaderMaterial
			mat_s = " shader=%s albedo_p=%s" % [
				sm.shader.resource_path if sm.shader else "?",
				sm.get_shader_parameter("albedo")
			]
		f.store_line("VIS %s layers=%s mesh=%s%s jvm=%s" % [
			str(mi.get_path()), mi.layers,
			mi.mesh.get_class() if mi.mesh else "null",
			mat_s, mi.has_meta("jvm_model")
		])
	f.store_line("visible_mesh_count=%s" % vis_n)
	f.store_line("=== full tree ===")
	dump_walk(f, lk, 0)
	f.close()
	p("dumped locker tree %s vis_meshes=%s path=%s" % [tag, vis_n, path])

func rotate_player_visual(lk: Node, yaw: float) -> void:
	var pl = lk.get("_player") if lk else null
	if pl == null or not (pl is Node):
		return
	var vis = pl.get("visual")
	if vis is Node3D:
		(vis as Node3D).rotation.y = yaw
		p("rotated visual.y=%s" % yaw)

func is_play_sv(sv: SubViewport) -> bool:
	var path := str(sv.get_path()).to_lower()
	if path.contains("main") or path.contains("canvaslayer") or path.contains("stage"):
		return true
	return sv.size.x >= 1280 and sv.size.y >= 720

func play_frame_ready() -> bool:
	var found_env := false
	var found_floor := false
	var found_court := false
	for n in get_tree().root.find_children("*", "Node", true, false):
		var nm := String(n.name)
		if n is Node3D and not (n as Node3D).is_visible_in_tree():
			continue
		if nm == "JVM_CourtEnv":
			found_env = true
		if nm == "CourtFloor" or nm == "EnvFloor":
			found_floor = true
		if nm == "Court":
			found_court = true
	for node in get_tree().root.find_children("*", "SubViewport", true, false):
		var sv := node as SubViewport
		if sv == null or not is_play_sv(sv):
			continue
		if sv.size.x >= 1280 and sv.find_children("*", "Camera3D", true, false).size() > 0:
			if found_court or found_env or found_floor:
				return true
	return found_court and (found_env or found_floor)

func wait_settled(tag: String, seconds: float = 18.0) -> void:
	var start := Time.get_ticks_msec()
	var ok := false
	while Time.get_ticks_msec() - start < int(seconds * 1000.0):
		await get_tree().create_timer(0.6).timeout
		if play_frame_ready():
			ok = true
			break
	await wait_frames(20)
	p("settled %s ok=%s after_ms=%s" % [tag, ok, Time.get_ticks_msec() - start])

func is_interesting_name(nm: String) -> bool:
	var interesting := ["Main", "School", "Gym", "Gymnasium", "Grass", "Beach", "Backdrop",
		"Seats", "Court", "CourtFloor", "EnvFloor", "JVM_CourtEnv", "Hills", "Trees",
		"Gym walls", "Ceiling", "Sunlight", "FloorGleam", "Stage", "Preview", "Lights",
		"WorldFill", "Coast", "City"]
	for k in interesting:
		if nm == k or nm.begins_with(k) or nm.strip_edges() == k or nm.strip_edges().begins_with(k):
			return true
	return false

func dump_tree(tag: String) -> void:
	var lines: Array = []
	var stack: Array = []
	var sc := get_tree().current_scene
	if sc:
		stack.append([sc, 0])
	while stack.size() > 0:
		var item = stack.pop_back()
		var n: Node = item[0]
		var depth: int = item[1]
		if n == null or depth > 10:
			continue
		var nm := String(n.name)
		var keep := is_interesting_name(nm) or depth <= 2 or (n is SubViewport) or (n is Camera3D)
		if keep:
			var vis := "?"
			if n is Node3D:
				vis = str((n as Node3D).visible) + "/tree=" + str((n as Node3D).is_visible_in_tree())
			var extra := ""
			if n is MeshInstance3D:
				extra = " mesh"
			if n is SubViewport:
				extra = " sv=%s" % (n as SubViewport).size
			var indent := ""
			for i in depth:
				indent += "  "
			lines.append("%s%s path=%s vis=%s%s" % [indent, nm, n.get_path(), vis, extra])
		if depth < 10:
			var kids := n.get_children()
			var i := kids.size() - 1
			while i >= 0:
				stack.append([kids[i], depth + 1])
				i -= 1
	for n in get_tree().root.find_children("JVM_CourtEnv", "Node", true, false):
		var vis := "?"
		if n is Node3D:
			vis = str((n as Node3D).is_visible_in_tree())
		var pp := "?"
		if n.get_parent():
			pp = str(n.get_parent().get_path())
		p("tree %s env_instance path=%s vis=%s parent=%s" % [tag, n.get_path(), vis, pp])
	var text := "\n".join(PackedStringArray(lines))
	p("tree %s begin" % tag)

	var chunk := 0
	while chunk < lines.size():
		var end := mini(chunk + 40, lines.size())
		var part := ""
		for i in range(chunk, end):
			part += str(lines[i]) + "\n"
		p("tree %s page\n%s" % [tag, part])
		chunk = end
	p("tree %s end lines=%d" % [tag, lines.size()])
	if out_dir != "":
		var f := FileAccess.open(out_dir.path_join("tree-%s.txt" % tag), FileAccess.WRITE)
		if f:
			f.store_string("\n".join(PackedStringArray(lines)))
			f.close()

func shot_play(name: String) -> void:
	await wait_frames(16)
	var vp := get_viewport()
	if vp and vp.get_texture():
		save_img(vp.get_texture().get_image(), name + "-window")
	var candidates: Array = []
	for node in get_tree().root.find_children("*", "SubViewport", true, false):
		var sv := node as SubViewport
		if sv == null:
			continue
		var cams := sv.find_children("*", "Camera3D", true, false)
		if cams.is_empty():
			continue
		candidates.append(sv)

	var ordered: Array = []
	while candidates.size() > 0:
		var best_i := 0
		var best_score := -1
		var best_area := -1
		for i in candidates.size():
			var sa := candidates[i] as SubViewport
			var score := 1 if is_play_sv(sa) else 0
			var area := sa.size.x * sa.size.y
			if score > best_score or (score == best_score and area > best_area):
				best_i = i
				best_score = score
				best_area = area
		ordered.append(candidates[best_i])
		candidates.remove_at(best_i)
	var n := 0
	for item in ordered:
		var sv := item as SubViewport
		if sv == null:
			continue
		await RenderingServer.frame_post_draw
		var tex = sv.get_texture()
		if tex:
			var label := "playcam-%d" % n
			if n == 0 and is_play_sv(sv) and sv.size.x >= 1280:
				label = "playcam-main"
			save_img(tex.get_image(), name + "-" + label)
			p("playcam name=%s path=%s size=%s cams=%s update_mode=%s" % [
				label, sv.get_path(), sv.size, sv.find_children("*", "Camera3D", true, false).size(),
				sv.render_target_update_mode])
			n += 1
	if n == 0:
		p("WARNING no SubViewport with Camera3D for %s" % name)

func renderer_info() -> void:
	var method := ""
	var driver := ""
	if RenderingServer.has_method("get_current_rendering_method"):
		method = str(RenderingServer.get_current_rendering_method())
	if RenderingServer.has_method("get_current_rendering_driver_name"):
		driver = str(RenderingServer.get_current_rendering_driver_name())
	var gpu := OS.get_environment("GPU")
	p("renderer method=%s driver=%s adapter=%s vendor=%s" % [
		method, driver,
		RenderingServer.get_video_adapter_name() if RenderingServer.has_method("get_video_adapter_name") else "?",
		RenderingServer.get_video_adapter_vendor() if RenderingServer.has_method("get_video_adapter_vendor") else "?"])
	var w := get_window()
	p("window mode=%s size=%s pos=%s content_scale=%s" % [w.mode, w.size, w.position, w.content_scale_factor])
	var sc := w.current_screen
	p("screen index=%s count=%s size=%s dpi=%s" % [
		sc, DisplayServer.get_screen_count(),
		DisplayServer.screen_get_size(sc),
		DisplayServer.screen_get_dpi(sc)])
	for i in DisplayServer.get_screen_count():
		p("screen[%d] size=%s pos=%s dpi=%s" % [
			i, DisplayServer.screen_get_size(i), DisplayServer.screen_get_position(i),
			DisplayServer.screen_get_dpi(i)])

func env_dump(tag: String) -> void:
	var tl := get_node_or_null("/root/JVTextureLoader")
	if tl == null:
		p("env %s: no loader" % tag)
		return
	var st: Dictionary = tl.get_env_status() if tl.has_method("get_env_status") else {}
	var sky: Dictionary = tl.get_sky_status() if tl.has_method("get_sky_status") else {}
	p("env %s status=%s sky=%s picked=%s" % [tag, str(st), str(sky), tl.get_picked_stage() if tl.has_method("get_picked_stage") else "?"])
	var floors := []
	var envs := []
	for n in get_tree().root.find_children("*", "Node", true, false):
		var nm := String(n.name)
		if nm == "EnvFloor" or nm == "CourtFloor" or nm == "JVM_CourtEnv":
			var vis := true
			if n is CanvasItem:
				vis = (n as CanvasItem).visible
			if n is Node3D:
				vis = (n as Node3D).visible
			var in_tree: bool = n.is_visible_in_tree()
			var vp = n.get_viewport()
			floors.append("%s path=%s vis=%s in_tree=%s vp=%s" % [nm, n.get_path(), vis, in_tree, vp.get_path() if vp else "?"])
	p("env %s nodes=%s" % [tag, str(floors)])
	dump_tree(tag)

func log_slots(tag: String) -> void:
	var gc = get_node_or_null("/root/GameConfig")
	var ch = gc.character if gc else null
	var sc = get_tree().current_scene
	var cfg = sc.get("cfg") if sc else null
	p("slots %s gc.head=%s gc.extra=%s cfg.head=%s cfg.extra=%s scene=%s" % [
		tag,
		ch.acc_id("head") if ch and ch.has_method("acc_id") else "?",
		ch.acc_id("extra") if ch and ch.has_method("acc_id") else "?",
		cfg.acc_id("head") if cfg and cfg.has_method("acc_id") else "?",
		cfg.acc_id("extra") if cfg and cfg.has_method("acc_id") else "?",
		sc.scene_file_path if sc else "?"])

func log_equipped_json(tag: String) -> void:
	var path := ProjectSettings.globalize_path("res://mods/resources/packs/equipped.json")
	if not FileAccess.file_exists(path):
		p("equipped.json %s MISSING %s" % [tag, path])
		return
	p("equipped.json %s %s" % [tag, FileAccess.get_file_as_string(path).strip_edges()])

func wait_menu() -> Node:

	var m: Node = null
	for i in 180:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null:
			continue
		if m.has_method("_on_continue") and not m.get("_left"):
			p("boot -> continue")
			m.call("_on_continue")
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

func stage_id_of(pack: Dictionary) -> String:
	return String(pack.get("id", pack.get("pack_id", "")))

func stage_label(pack: Dictionary) -> String:
	var s := String(pack.get("label", pack.get("name", stage_id_of(pack))))
	if s == "":
		s = stage_id_of(pack)
	return s

func slug(s: String) -> String:
	var t := s.to_lower().replace(" ", "-")
	var out := ""
	for ch in t:
		var o := ch.unicode_at(0)
		if (o >= 97 and o <= 122) or (o >= 48 and o <= 57) or ch == "-":
			out += ch
	return out if out != "" else "x"

func pick_stage(tl: Node, id: String) -> bool:
	if tl == null or not tl.has_method("set_picked_stage"):
		return false
	var ok = tl.set_picked_stage(id)
	if tl.has_method("apply_picked_cosmetics"):
		tl.apply_picked_cosmetics()
	var gc = get_node_or_null("/root/GameConfig")
	var menu = get_tree().current_scene if get_tree() else null
	p("after pick %s fp_stage=%s fp_pick=%s menu_fp=%s" % [
		id,
		gc.get("freeplay_stage") if gc else "?",
		menu.get("_fp_pick") if menu and "_fp_pick" in menu else "?",
		menu.get("_freeplay_stage") if menu and "_freeplay_stage" in menu else "?"
	])
	return bool(ok)

func press_acc(lk: Node, cat: String, id: String) -> bool:
	var subs: Dictionary = lk.get("_sub_btns") as Dictionary
	if subs.has(cat) and subs[cat] is Button:
		(subs[cat] as Button).pressed.emit()
		await get_tree().create_timer(1.5).timeout
	var acc_btns: Dictionary = lk.get("_acc_buttons") as Dictionary
	if acc_btns.has(id) and acc_btns[id] is Button:
		(acc_btns[id] as Button).pressed.emit()
		await get_tree().create_timer(2.0).timeout
		return true
	p("no acc button %s in %s keys=%s" % [id, cat, str(acc_btns.keys())])
	return false

func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
	var phase := OS.get_environment("JV_PROBE_PHASE")
	if phase == "":
		phase = "all"
	p("start out=%s phase=%s" % [out_dir, phase])
	var m := await wait_menu()
	if m == null:
		p("FAIL no menu")
		get_tree().quit(3)
		return
	renderer_info()
	var tl: Node = get_node_or_null("/root/JVTextureLoader")
	if tl == null:
		p("FAIL no JVTextureLoader")
		get_tree().quit(6)
		return
	p("status=%s" % str(tl.get_status() if tl.has_method("get_status") else {}))
	var stages: Array = tl.list_picker_packs("stage") if tl.has_method("list_picker_packs") else []
	p("stage_packs=%s" % str(stages))


	if phase != "locker":
		var royal_ok := pick_stage(tl, "")
		p("pick Royal/clear=%s" % royal_ok)
		await wait_settled("royal-menu", 8.0)
		env_dump("royal-menu")
		await shot_play("court-royal-menu")

		if m.has_method("_try_freeplay"):
			m.call("_try_freeplay")
			await wait_settled("royal-freeplay", 22.0)
			env_dump("royal-freeplay")
			await shot_play("court-royal-freeplay")

			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn")
			m = await wait_menu()
			if m == null:
				p("FAIL menu after royal freeplay")
				get_tree().quit(7)
				return

		for pack in stages:
			var id := stage_id_of(pack)
			if id == "":
				continue
			var tag := "%s-%s" % [id, slug(stage_label(pack))]
			var ok := pick_stage(tl, id)
			p("pick %s (%s) ok=%s" % [id, stage_label(pack), ok])
			await wait_settled("menu-" + tag, 8.0)
			env_dump("menu-" + tag)
			await shot_play("court-%s-menu" % tag)

			var info: Dictionary = tl.pack_info(id) if tl.has_method("pack_info") else {}
			var want_fp := bool(info.get("has_environment", false)) or id in ["1023", "1026", "1027", "1005"]
			if want_fp and m and m.has_method("_try_freeplay"):
				m.call("_try_freeplay")
				var settle_s := 24.0 if id in ["1023", "1005"] else 18.0
				await wait_settled("freeplay-" + tag, settle_s)
				env_dump("freeplay-" + tag)
				await shot_play("court-%s-freeplay" % tag)
				get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn")
				m = await wait_menu()
				if m == null:
					p("FAIL menu after freeplay %s" % id)
					break

		if phase == "courts":
			p("done phase=courts")
			get_tree().quit(0)
			return


	var gc = get_node("/root/GameConfig")
	var ch = gc.character

	if ch and ch.has_method("set_acc_id"):
		ch.set_acc_id("head", "")
		ch.set_acc_id("head2", "")
		ch.set_acc_id("face", "")
		ch.set_acc_id("extra", "")
	gc.goto_scene("res://game/character/locker.tscn")
	var lk: Node = null
	for i in 40:
		await get_tree().create_timer(0.5).timeout
		lk = get_tree().current_scene
		if lk and lk.scene_file_path == "res://game/character/locker.tscn":
			var rail0: Dictionary = lk.get("_rail_btns") as Dictionary
			if rail0.has("accessories"):
				break
	if lk == null or lk.scene_file_path != "res://game/character/locker.tscn":
		p("FAIL locker not opened")
		get_tree().quit(4)
		return
	var rail: Dictionary = lk.get("_rail_btns") as Dictionary
	p("locker rail=%s" % str(rail.keys()))
	if not rail.has("accessories"):
		p("FAIL no accessories rail")
		get_tree().quit(5)
		return
	(rail["accessories"] as Button).pressed.emit()
	await get_tree().create_timer(2.0).timeout
	if tl.has_method("get_accessory_status"):
		p("accessory_status=%s" % str(tl.get_accessory_status()))


	var subs: Dictionary = lk.get("_sub_btns") as Dictionary
	if subs.has("head"):
		(subs["head"] as Button).pressed.emit()
		await get_tree().create_timer(3.0).timeout
	await shot_window("locker-captain-head-unequipped")
	log_slots("captain-before-cap")
	log_equipped_json("before")


	await press_acc(lk, "head", "1002")
	log_slots("captain-cap-only")
	await shot_window("locker-captain-head-cap-only")


	await press_acc(lk, "extra", "1003")
	log_slots("captain-together")
	if (lk.get("_sub_btns") as Dictionary).has("head"):
		((lk.get("_sub_btns") as Dictionary)["head"] as Button).pressed.emit()
		await get_tree().create_timer(2.0).timeout
	await shot_window("locker-captain-together-head-tab")
	if (lk.get("_sub_btns") as Dictionary).has("extra"):
		((lk.get("_sub_btns") as Dictionary)["extra"] as Button).pressed.emit()
		await get_tree().create_timer(2.0).timeout
	dump_locker(lk, "captain-together-extra")
	rotate_player_visual(lk, 0.9)
	await get_tree().create_timer(0.6).timeout
	await shot_window("locker-captain-together-extra-tab-three-quarter")
	rotate_player_visual(lk, 3.14159)
	await get_tree().create_timer(0.6).timeout
	await shot_window("locker-captain-together-extra-tab-back")
	rotate_player_visual(lk, 0.0)
	await get_tree().create_timer(0.4).timeout
	await shot_window("locker-captain-together-extra-tab")
	log_equipped_json("together")


	var cfg = lk.get("cfg") if lk else null
	if cfg and cfg.has_method("set_acc_id"):
		cfg.set_acc_id("head", "")
	if ch and ch.has_method("set_acc_id"):
		ch.set_acc_id("head", "")
	if lk.has_method("_changed"):
		lk.call("_changed")
	elif lk.has_method("_sync_controls"):
		lk.call("_sync_controls")
	if (lk.get("_sub_btns") as Dictionary).has("head"):
		((lk.get("_sub_btns") as Dictionary)["head"] as Button).pressed.emit()
		await get_tree().create_timer(2.0).timeout
	log_slots("captain-tail-only")
	await shot_window("locker-captain-head-tail-only")
	if (lk.get("_sub_btns") as Dictionary).has("extra"):
		((lk.get("_sub_btns") as Dictionary)["extra"] as Button).pressed.emit()
		await get_tree().create_timer(2.0).timeout
	await shot_window("locker-captain-extra-tail-only")


	var ml: Node = get_node_or_null("/root/JVModelLoader")
	var bean_id := ""
	if ml and ml.has_method("get_model_list"):
		var models = ml.get_model_list()
		p("model_list=%s saved=%s shown=%s" % [str(models), ml.get_saved_model() if ml.has_method("get_saved_model") else "?", ml.get_shown_model() if ml.has_method("get_shown_model") else "?"])
		if models is Array:
			for item in models:
				var mid := ""
				var label := ""
				if item is Dictionary:
					mid = str(item.get("id", item.get("pack_id", "")))
					label = str(item.get("name", item.get("label", ""))).to_lower()
				else:
					mid = str(item)
				if mid.to_upper() == "BEA0" or "bean" in label or "plump" in label or mid.to_lower().contains("bean"):
					bean_id = mid
					break
			if bean_id == "" and models.size() > 0:
				var first = models[0]
				bean_id = str(first.get("id", first)) if first is Dictionary else str(first)
				p("WARNING no bean name match; using first model %s" % bean_id)
		if bean_id == "":
			bean_id = "BEA0"
			p("model_list empty; defaulting to BEA0")
	if rail.has("jvm_full_model"):
		(rail["jvm_full_model"] as Button).pressed.emit()
		await get_tree().create_timer(2.0).timeout
		if ml and bean_id != "" and ml.has_method("preview_model"):
			p("preview bean id=%s" % bean_id)
			ml.preview_model(bean_id)
			await get_tree().create_timer(0.8).timeout
		var content = lk.get("_content")
		if content is Node:
			for b in (content as Node).find_children("*", "Button", true, false):
				var txt := str(b.text).to_lower()
				if "plump" in txt or ("bean" in txt and "vanilla" not in txt):
					p("clicking model tile '%s'" % b.text)
					(b as Button).pressed.emit()
					await get_tree().create_timer(1.2).timeout
					break
		await get_tree().create_timer(1.0).timeout
		var banner = ""
		var ob = lk.get("_outfit_btn")
		if ob is Button:
			banner = ob.text
		p("bean fullmodel shown=%s banner=%s" % [ml.get_shown_model() if ml and ml.has_method("get_shown_model") else "?", banner])
		dump_locker(lk, "bean-fullmodel")
		await shot_window("locker-bean-fullmodel")

		if rail.has("accessories"):
			(rail["accessories"] as Button).pressed.emit()
			await get_tree().create_timer(1.5).timeout
		await press_acc(lk, "head", "1002")
		log_slots("bean-cap-only")
		await shot_window("locker-bean-head-cap-only")
		await press_acc(lk, "extra", "1003")
		log_slots("bean-together")
		if (lk.get("_sub_btns") as Dictionary).has("head"):
			((lk.get("_sub_btns") as Dictionary)["head"] as Button).pressed.emit()
			await get_tree().create_timer(2.0).timeout
		await shot_window("locker-bean-together-head-tab")
		if (lk.get("_sub_btns") as Dictionary).has("extra"):
			((lk.get("_sub_btns") as Dictionary)["extra"] as Button).pressed.emit()
			await get_tree().create_timer(2.0).timeout
		await shot_window("locker-bean-together-extra-tab")
		dump_locker(lk, "bean-together-extra")
		rotate_player_visual(lk, 0.9)
		await get_tree().create_timer(0.6).timeout
		await shot_window("locker-bean-together-extra-tab-three-quarter")
		rotate_player_visual(lk, 3.14159)
		await get_tree().create_timer(0.6).timeout
		await shot_window("locker-bean-together-extra-tab-back")
		rotate_player_visual(lk, 0.0)
		await get_tree().create_timer(0.4).timeout
		if rail.has("jvm_full_model"):
			(rail["jvm_full_model"] as Button).pressed.emit()
			await get_tree().create_timer(2.5).timeout
			dump_locker(lk, "bean-fullmodel-with-acc")
			await shot_window("locker-bean-fullmodel-with-acc")
			rotate_player_visual(lk, 0.9)
			await get_tree().create_timer(0.6).timeout
			await shot_window("locker-bean-fullmodel-with-acc-three-quarter")
			rotate_player_visual(lk, 3.14159)
			await get_tree().create_timer(0.6).timeout
			await shot_window("locker-bean-fullmodel-with-acc-back")
			rotate_player_visual(lk, 0.0)
		if ml and ml.has_method("end_preview"):
			ml.end_preview()
	else:
		p("WARNING no jvm_full_model rail; cannot prove 03:41 bean case")


	cfg = lk.get("cfg") if lk else null
	if cfg and ch and cfg.has_method("acc_id"):
		for slot in ["head", "head2", "face", "extra"]:
			if ch.has_method("set_acc_id"):
				ch.set_acc_id(slot, cfg.acc_id(slot))
		if ch.has_method("save_local"):
			ch.save_local()
	log_slots("final")

	get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn")
	m = await wait_menu()
	await get_tree().create_timer(1.5).timeout
	log_slots("after-locker-exit")
	log_equipped_json("after-exit")
	p("done")
	get_tree().quit(0)
