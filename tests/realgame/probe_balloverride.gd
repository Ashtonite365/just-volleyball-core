extends Node


func p(msg: String) -> void: print("PROBE ", msg)

func shot(tag: String) -> void:
	if OS.get_environment("JV_PROBE_OUT") == "": return
	for i in 6: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var path := OS.get_environment("JV_PROBE_OUT").path_join("%s.png" % tag)
	get_viewport().get_texture().get_image().save_png(path)
	p("screenshot=%s" % path)

func wait_menu() -> Node:
	var m: Node = null
	for i in 120:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"):
			m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_card_btn") and m.find_child("JvmModsCard", true, false) != null:
			await get_tree().create_timer(1.0).timeout
			break
	return m

func bar_btn(m: Node, prefix: String) -> Button:
	var bar = m.get("_bottom_bar")
	if bar == null: return null
	for c in bar.get_children():
		if c is Button and String(c.text).begins_with(prefix): return c
	return null

func row() -> Control: return get_tree().root.find_child("JvmBallOverride", true, false)
func row_shown() -> bool: return row() != null and row().is_visible_in_tree()
func row_btn(t: String) -> Button:
	if row() == null: return null
	for b in row().find_children("*", "Button", true, false):
		if b.text == t: return b
	return null

func _ready() -> void:
	var m := await wait_menu()
	if m == null: p("FAIL no menu"); get_tree().quit(3); return
	var g = get_node_or_null("/root/JVGoofyMods")
	var gate = get_node_or_null("/root/JVModGate")
	p("goofy=%s api=%s mode=%s gate_mode=%s physics_allowed=%s" % [g != null, g.api_version() if g else "?", g.get_ball_mode() if g else "?", gate.get_mode() if gate else "?", g.is_physics_allowed() if g else "?"])
	var stagebtn := bar_btn(m, "stage:")
	stagebtn.pressed.emit(); await get_tree().create_timer(1.0).timeout
	var pk = m.get("_fp_picker")
	var grid: Node = pk.get("_grid")
	var stock: Button = null; var royal: Button = null; var test: Button = null
	for c in grid.get_children():
		if not (c is Button): continue
		var id := String(c.get_meta("jvm_stage_pack", ""))
		if id == "" and stock == null: stock = c
		if id == "1005": royal = c
		if id == "1090": test = c
	p("tiles stock=%s royal=%s test=%s" % [stock != null, royal != null, test != null])
	stock.pressed.emit(); await get_tree().create_timer(0.6).timeout
	p("stock map highlighted: row_exists=%s row_shown=%s" % [row() != null, row_shown()])
	await shot("bo-1-stock-map-hidden")
	test.pressed.emit(); await get_tree().create_timer(0.6).timeout
	p("custom map highlighted: row_shown=%s" % row_shown())
	await shot("bo-2-custom-map-normal")
	row_btn("fixed").pressed.emit(); await get_tree().create_timer(0.4).timeout
	p("pressed fixed: mode=%s" % g.get_ball_mode())
	await shot("bo-3-custom-map-fixed")
	stock.pressed.emit(); await get_tree().create_timer(0.6).timeout
	p("back to stock tile: row_shown=%s" % row_shown())
	test.pressed.emit(); await get_tree().create_timer(0.6).timeout
	for b in pk.find_children("*", "Button", true, false):
		if b.text.to_lower() == "done": b.pressed.emit(); break
	await get_tree().create_timer(1.5).timeout
	var ph = load("res://game/ball/ball_physics.tres"); var feel = load("res://game/player/player_feel.tres")
	p("after done (test court, FIXED): loader_map=%s force_defaults=%s gravity=%.2f reach=%.2f layers=%s" % [g.get_loader_map(), g.get_force_defaults(), ph.gravity, feel.reach, str(g.get_layers())])
	g.set_ball_mode("normal"); await get_tree().create_timer(0.6).timeout
	p("NORMAL on test court: gravity=%.2f (stock 11 x0.5) reach=%.2f (court 1.6) params=%s" % [ph.gravity, feel.reach, str(g.get_params())])
	var f := FileAccess.get_file_as_string(OS.get_executable_path().get_base_dir().path_join("mods/goofy/state/ball_override.json"))
	p("ball_override.json=%s" % f.replace("\n", " "))
	p("done")
	get_tree().quit(0)
