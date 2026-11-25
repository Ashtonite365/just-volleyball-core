extends Node


func p(msg: String) -> void: print("PROBE ", msg)
func shot(tag: String) -> void:
	if OS.get_environment("JV_PROBE_SHOT") == "": return
	for i in 6: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var out := OS.get_environment("JV_PROBE_SHOT").replace(".png", "-%s.png" % tag)
	get_viewport().get_texture().get_image().save_png(out); p("screenshot=%s" % out)


func balls_report(tag: String) -> void:
	var phys = load("res://game/ball/ball_physics.tres")
	var out := []
	for n in get_tree().root.find_children("*", "Node3D", true, false):
		var sc = n.get_script()
		if sc == null or sc.get_global_name() != "Ball": continue
		var model = n.get("_model"); var vis := -1.0
		if model is Node3D and model.is_inside_tree():
			var inv: Transform3D = model.global_transform.affine_inverse(); var box := AABB(); var first := true
			for mi in model.find_children("*", "MeshInstance3D", true, false):
				if mi.mesh == null: continue
				var b: AABB = (inv * mi.global_transform) * mi.get_aabb()
				box = b if first else box.merge(b); first = false
			if not first: vis = box.get_longest_axis_size() * 0.5 * n.global_basis.get_scale().x
		out.append("%s scale=%.3f visual_r=%.3f" % [n.name, n.scale.x, vis])
	p("balls[%s] phys_radius=%.3f gravity=%.2f bounce=%.2f | %s" % [tag, phys.radius, phys.gravity, phys.floor_bounce, " ; ".join(out)])






func air_shots(m: Node) -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS


	var host: Node = m
	for n in get_tree().root.find_children("*", "Node", true, false):
		if n == m or n.get("camera_rig") == null: continue
		if n.get("player") is Node3D and n.get("ball") is Node3D and (n.get("player") as Node3D).is_visible_in_tree():
			host = n; break
	p("airshots host=%s (%s)" % [host.get_path(), host.get_script().resource_path if host.get_script() else "?"])
	var pl = host.get("player"); var b = host.get("ball")
	var sh = get_node_or_null("/root/JVExtraShots")
	if pl == null or b == null or sh == null: p("airshots: no player/ball/shots"); return
	p("airshots court: center_x=%s half_w=%s half_l=%s dome=%s walls=%s..%s" % [b.get("court_center_x"), b.get("court_half_w"), b.get("court_half_l"), b.get("dome"), b.get("wall_min"), b.get("wall_max")])
	for spec in [["topspin", "move_forward"], ["curveball", "move_right"], ["curveball2", "move_left"], ["float", "move_back"]]:
		Engine.time_scale = 1.0
		b.visible = true; b.frozen = false

		var side := signf(pl.global_position.z) if absf(pl.global_position.z) > 0.5 else 1.0

		pl.global_position = Vector3(pl.global_position.x, pl.global_position.y + 1.6, side * 3.0); pl.velocity = Vector3.ZERO; pl.move_and_slide()
		var aim := Vector3(pl.global_position.x, 0.0, -side * 5.0)
		pl.aim_point = aim
		b.global_position = pl.global_position + Vector3(0, 2.0, -side * 0.4)
		b.strike_source = pl; b.set_meta("from", b.global_position)
		Input.action_press(spec[1])
		var air: bool = not pl.grounded()
		b.hit(aim, 3, 0.45 if spec[0] == "curveball2" else 0.8, false)
		Input.action_release(spec[1])
		for i in 4: await get_tree().physics_frame
		get_tree().paused = true
		var lp = b.get("_land_predict"); var rb: Dictionary = sh.get_range_box()
		var exact: Vector3 = b._predict_land_curved()
		p("airshot[%s] player=%s ball_from=%s airborne=%s event=%s marker_visible=%s marker=%s exact_landing=%s float_serve=%s range_box_on=%s box_center=%s box_fade=%.2f spread=%.2f box=%s last_spin=%s" % [spec[0], pl.global_position, b.get_meta("from"), air, sh.get_last_event(),
			lp.visible if lp else "?", lp.global_position if lp else "?", exact, b.float_serve, rb.get("on"), rb.get("center"), float(rb.get("fade", -1)), float(b.get("_predict_spread")), str(rb), str(sh.get_last_spin()) if spec[0].begins_with("curveball") else ""])
		await shot("air-" + spec[0])

		var vp: Viewport = b.get_viewport(); var cam: Camera3D = vp.get_camera_3d()
		if OS.get_environment("JV_PROBE_SHOT") != "" and cam:
			var out := OS.get_environment("JV_PROBE_SHOT").replace(".png", "-court-%s.png" % spec[0])
			vp.get_texture().get_image().save_png(out)
			p("courtshot[%s]=%s size=%s marker_px=%s exact_px=%s box_center_px=%s" % [spec[0], out, vp.get_visible_rect().size, cam.unproject_position(lp.global_position), cam.unproject_position(exact), cam.unproject_position(rb.get("center", Vector3.ZERO))])

			var look: Vector3 = Vector3(lp.global_position.x, 0.0, lp.global_position.z)
			var top := Camera3D.new(); vp.add_child(top)
			top.look_at_from_position(look + Vector3(0, 7.0, 4.5), look, Vector3.UP); top.current = true
			p("topshot[%s] before: box=%s exact=%s ball=%s player=%s | px box=%s exact=%s ball=%s player=%s" % [spec[0], lp.global_position, exact, b.global_position, pl.global_position, top.unproject_position(lp.global_position), top.unproject_position(Vector3(exact.x, 0.05, exact.z)), top.unproject_position(b.global_position), top.unproject_position(pl.global_position)])
			for i in 6: await get_tree().process_frame
			await RenderingServer.frame_post_draw
			var out2 := OS.get_environment("JV_PROBE_SHOT").replace(".png", "-top-%s.png" % spec[0])
			vp.get_texture().get_image().save_png(out2)
			p("topshot[%s]=%s after: box=%s px=%s exact_px=%s ball=%s range_box=%s" % [spec[0], out2, lp.global_position, top.unproject_position(lp.global_position), top.unproject_position(Vector3(exact.x, 0.05, exact.z)), b.global_position, str(sh.get_range_box())])
			top.current = false; cam.current = true; top.queue_free()
		get_tree().paused = false
		await get_tree().create_timer(2.5).timeout
	Engine.time_scale = 1.0
func _ready() -> void:


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
	p("waited for menu")
	p("scene=%s script=%s" % [m.scene_file_path if m else "?", m.get_script().resource_path if m and m.get_script() else "?"])
	if m == null: get_tree().quit(3); return
	var home: Control = m.get("_home")
	var bar: Node = m.get("_bottom_bar")
	var discord: Node = m.get("_discord_btn")
	var card = m.find_child("JvmModsCard", true, false)
	var credits = m.find_child("JvmCreditsButton", true, false)
	p("home=%s bar=%s discord=%s" % [home != null, bar != null, discord != null])
	p("mods_card=%s slot=%d of %d" % [card != null, card.get_index() if card else -1, card.get_parent().get_child_count() if card else -1])
	p("credits_in_bar=%s right_of_discord=%s" % [credits != null and credits.get_parent() == bar, credits != null and discord != null and credits.get_index() == discord.get_index() + 1])
	var tilt: Array = m.get("_tilt")
	p("credits_in_tilt=%s mods_in_tilt=%s" % [tilt.has(credits), card != null and tilt.has(card.get_child(0))])

	var names := []
	if card:
		for w in card.get_parent().get_children():
			if w.is_queued_for_deletion() or w.get_child_count() == 0 or not (w.get_child(0) is Button): continue
			var ls: Array = w.get_child(0).find_children("*", "Label", true, false)
			names.append(ls[0].text if ls.size() else "?")
	p("home_order=" + ",".join(names))
	if credits:
		var sb = credits.get_theme_stylebox("normal")
		p("credits_class=%s text=%s icon=%s yellow=%s visible=%s rect=%s" % [credits.get_class(), credits.text, credits.icon != null,
			sb is StyleBoxFlat and sb.bg_color.is_equal_approx(Color("ffd23f")), credits.is_visible_in_tree(), credits.get_global_rect()])
		p("credits_focus_top=%s left=%s" % [credits.focus_neighbor_top, credits.focus_neighbor_left])
		p("credits_icon=%s not_discord_logo=%s" % [credits.icon.resource_path if credits.icon else "none", credits.icon != (discord as Button).icon])
		p("mods_card_h=%d practice_card_h=%d" % [card.get_child(0).custom_minimum_size.y, (m.get("_tilt")[2] as Control).custom_minimum_size.y])
		balls_report("menu-default")
		p("discord_rect=%s" % (discord as Control).get_global_rect())

		var last: Button = null
		for w in card.get_parent().get_children():
			if w.get_child_count() and w.get_child(0) is Button and w.get_child(0).is_visible_in_tree(): last = w.get_child(0)
		var fo := func() -> String:
			var f = get_viewport().gui_get_focus_owner()
			if f == null: return "none"
			if f.name == "JvmCreditsButton": return "credits"
			if f == discord: return "discord"
			var ls: Array = f.find_children("*", "Label", true, false)
			return (ls[0].text if ls.size() else (f.text if f is Button and f.text != "" else str(f.name)))
		last.grab_focus()
		var same_frame: String = fo.call()
		await get_tree().process_frame
		var chain := []
		var n: Node = last
		while n:
			if n is Control: chain.append("%s:fb=%s,fm=%d" % [n.name, n.get("focus_behavior_recursive"), (n as Control).focus_mode])
			n = n.get_parent()
		p("focus_chain=" + " < ".join(chain.slice(0, 6)))
		var tb := Button.new(); var cl := CanvasLayer.new(); get_tree().root.add_child(cl); cl.add_child(tb); tb.grab_focus()
		p("fresh_button_focus=%s window_focus=%s input_disabled=%s" % [tb.has_focus(), get_window().has_focus(), get_viewport().is_input_disabled()])
		cl.queue_free()
		p("nav_ctx freeplay=%s practice=%s home_visible=%s modal=%s same_frame_focus=%s" % [m.get("_freeplay"), m.get("_practice_on"), (m.get("_home") as Control).is_visible_in_tree(), m.call("_modal_up"), same_frame])
		var path := ["start:" + fo.call() + "(focusable=%s)" % (last.focus_mode != Control.FOCUS_NONE)]
		for d in ["down", "right", "right", "right", "left", "up"]:
			m.call("_nav_move", d); await get_tree().process_frame
			path.append(d + ">" + fo.call())
		p("pad_nav=" + " ".join(path))
		get_viewport().gui_release_focus()

		var r0: Rect2 = credits.get_global_rect(); var moved := false
		var vp := get_viewport().get_visible_rect().size
		for i in 24:
			var ev := InputEventMouseMotion.new(); ev.position = Vector2(vp.x * (i % 6) / 5.0, vp.y * (i / 6) / 3.0)
			Input.parse_input_event(ev); get_viewport().warp_mouse(ev.position)
			await get_tree().process_frame
			if credits.get_global_rect() != r0 or credits.rotation != 0.0 or (credits.offset_transform_enabled and credits.offset_transform_rotation != 0.0): moved = true
		p("credits_stationary=%s" % (not moved))
		var sz := OS.get_environment("JV_PROBE_SIZE")
		if sz != "":
			var w := get_window()
			p("window before: size=%s mode=%d content_scale=%s" % [w.size, w.mode, w.content_scale_size])
			w.mode = Window.MODE_WINDOWED; w.size = Vector2i(int(sz.split("x")[0]), int(sz.split("x")[1])); w.position = Vector2i.ZERO
			for i in 30: await get_tree().process_frame
			p("window after: size=%s visible_rect=%s" % [w.size, get_viewport().get_visible_rect()])
			p("credits_rect=%s discord_rect=%s bar_rect=%s" % [credits.get_global_rect(), (discord as Control).get_global_rect(), (bar as Control).get_global_rect()])
			for i in 2:
				var ev := InputEventMouseMotion.new(); ev.position = Vector2(w.size.x * (0.15 + 0.7 * i), w.size.y * 0.3)
				Input.parse_input_event(ev); get_viewport().warp_mouse(ev.position)
				for k in 20: await get_tree().process_frame
				await RenderingServer.frame_post_draw
				var img := get_viewport().get_texture().get_image()
				var out := OS.get_environment("JV_PROBE_SHOT").replace(".png", "-mouse%d.png" % i)
				img.save_png(out); p("screenshot=%s size=%s credits_rect=%s" % [out, img.get_size(), credits.get_global_rect()])
		credits.pressed.emit(); await get_tree().process_frame; await get_tree().process_frame
		var cp = m.get("_credits_page")
		p("credits_opens=%s" % (cp != null and is_instance_valid(cp)))
		await shot("credits")
		if cp and is_instance_valid(cp): cp.queue_free(); m.set("_credits_page", null)
		await get_tree().process_frame
	if card:
		var b: Button = card.get_child(0)
		p("mods_focus_top=%s bottom=%s" % [b.focus_neighbor_top, b.focus_neighbor_bottom])
		b.pressed.emit()
		await get_tree().process_frame; await get_tree().process_frame
		var panel = m.find_child("JvmModsPanel", true, false)
		p("mods_panel=%s" % (panel != null))
		if panel:
			p("panel_text=" + (panel.find_child("Body", true, false) as Label).text.replace("\n", " | "))
			var close = panel.find_child("JvmModsClose", true, false)
			p("close_button=%s" % (close != null))
			p("panel_modal=%s modal_up=%s" % [m.get("_guide_page") == panel, m.call("_modal_up")])
			await shot("modslist")
			var mu = get_node("/root/JVModGate/JVMenuUI")
			var online: Button = m.get("_tilt")[0]
			for i in 6:
				var ev := InputEventMouseMotion.new(); ev.position = online.get_global_rect().get_center(); ev.relative = Vector2(1, 0)
				Input.parse_input_event(ev); await get_tree().process_frame
			p("behind_card_z=%d offset=%s mouse_filter=%d" % [online.z_index, online.get("offset_transform_enabled"), online.mouse_filter])
			mu.show_detail("goofy"); await get_tree().process_frame; await get_tree().process_frame
			var spins: Array = panel.find_children("*", "SpinBox", true, false)
			var bb = null
			for w in spins: if w.has_meta("jvm_key") and Array(w.get_meta("jvm_key")) == ["big_ball"]: bb = w
			p("detail_view=%s fields=%d big_ball_field=%s" % [mu.get_info().get("view"), panel.find_children("*", "Control", true, false).filter(func(c): return c.has_meta("jvm_key")).size(), bb != null])
			if bb:
				bb.value = 1.2
				var msg: String = mu.save_detail()
				await get_tree().process_frame
				var gf := "res://mods/goofy/00-default.json"
				p("save_msg=" + msg.replace("\n", " | "))
				await get_tree().create_timer(0.7).timeout
				balls_report("after-editor-big_ball-1.2")
				bb.value = 99.0
				var bad: String = mu.save_detail()
				p("validator_refused=%s msg=%s" % [bad.begins_with("not saved") and bad.contains("big_ball must be between"), bad.split("\n")[0]])
				bb.value = 1.2
				p("saved_big_ball=%s bak=%s goofy_big_ball=%s" % [JSON.parse_string(FileAccess.get_file_as_string(ProjectSettings.globalize_path(gf).get_base_dir().path_join("00-default.json")))["big_ball"] if FileAccess.file_exists(gf) else "?",
					FileAccess.file_exists(gf + ".bak"), get_node("/root/JVGoofyMods").get_status().get("features", {}).get("big_ball")])
			if OS.get_environment("JV_PROBE_SHOT") != "":
				await RenderingServer.frame_post_draw
				var img := get_viewport().get_texture().get_image()
				var out := OS.get_environment("JV_PROBE_SHOT").replace(".png", "-detail.png")
				img.save_png(out); p("screenshot=%s" % out)
			var ce := InputEventAction.new(); ce.action = "ui_cancel"; ce.pressed = true; Input.parse_input_event(ce)
			await get_tree().process_frame; ce = InputEventAction.new(); ce.action = "ui_cancel"; ce.pressed = false; Input.parse_input_event(ce); await get_tree().process_frame
			p("esc_back_to=%s" % mu.get_info().get("view"))
			close = panel.find_child("JvmModsClose", true, false)
			if close: close.pressed.emit()
			await get_tree().process_frame; await get_tree().process_frame
			p("panel_closed=%s modal_up_after=%s focus=%s" % [m.find_child("JvmModsPanel", true, false) == null, m.call("_modal_up"), get_viewport().gui_get_focus_owner() == card.get_child(0)])

	p("move_forward_events=%d jvm_forward=%s" % [InputMap.action_get_events("move_forward").size(), InputMap.has_action("jvm_forward")])
	var ib = get_node_or_null("/root/InputBinds")
	if ib and ib.has_method("apply"):
		ib.apply({})
		await get_tree().process_frame
		p("after_game_apply_forward_events=%d" % InputMap.action_get_events("move_forward").size())
		await get_tree().process_frame; await get_tree().process_frame
		p("after_settle_forward_events=%d" % InputMap.action_get_events("move_forward").size())
	var sh = get_node_or_null("/root/JVExtraShots")
	if sh: p("shots=" + str(sh.get_status()))
	var g = get_node_or_null("/root/JVGoofyMods")
	if g: p("goofy=" + str(g.get_status().get("features")))

	if m.has_method("_open_modifiers"):
		m.call("_open_modifiers", false)
		await get_tree().create_timer(0.8).timeout
		var mp = m.find_child("ModifiersPanel", true, false)
		p("modifiers_panel=%s" % (mp != null))
		if mp:
			var rows: Dictionary = mp.get("_rows")
			p("modifier_rows=%s" % str(rows.keys()))
			var sel = rows.get("jvm_max_hits")
			if sel:
				p("max_hits_options=%s selected=%s class=%s" % [str(sel.options), sel.selected, sel.get_class()])
				sel.picked.emit(4); await get_tree().process_frame
				p("goofy_max_hits_after_pick=%d" % g.get_max_hits())
			var cb = rows.get("jvm_no_double")
			if cb:
				cb.button_pressed = true; await get_tree().process_frame
				p("goofy_no_double_after_toggle=%s" % g.get_no_double_hit())

	if OS.get_environment("JV_PROBE_SHOT") != "" and is_instance_valid(m):
		var gf := "res://mods/goofy/00-default.json"
		if FileAccess.file_exists(gf + ".bak"):
			var t := FileAccess.get_file_as_string(gf + ".bak"); var f := FileAccess.open(gf, FileAccess.WRITE); f.store_string(t); f.close()
			get_node("/root/JVGoofyMods").reload_config(); await get_tree().create_timer(0.7).timeout
		var mp2 = m.find_child("ModifiersPanel", true, false)
		if mp2 and mp2.has_method("close"): mp2.call("close"); await get_tree().create_timer(0.5).timeout
		if m.has_method("_try_freeplay"):
			p("freeplay: the menu's own freeplay button path (_try_freeplay)")
			m.call("_try_freeplay"); await get_tree().create_timer(4.0).timeout
			balls_report("freeplay"); await shot("freeplay")
			await air_shots(m)

		if OS.get_environment("JV_PROBE_MATCH") != "" and m.has_method("_go_to_match"):
			if m.has_method("_exit_freeplay"): m.call("_exit_freeplay"); await get_tree().create_timer(1.0).timeout
			m.call("_go_to_match", 1)
			for i in 40:
				await get_tree().create_timer(0.5).timeout
				var cs = get_tree().current_scene
				if cs and cs.scene_file_path == "res://game/main/main.tscn" and i > 14: break
			p("match_scene=%s" % (get_tree().current_scene.scene_file_path if get_tree().current_scene else "?"))
			balls_report("match"); await shot("match")
	p("done")
	get_tree().quit(0)
