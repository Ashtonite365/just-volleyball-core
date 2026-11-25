extends Node



func p(msg: String) -> void: print("PROBE ", msg)
func tl() -> Node: return get_node_or_null("/root/JVTextureLoader")

func _ready() -> void:
	var m: Node = null
	for i in 150:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"):
			m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_card_btn") and m.get("player") != null:
			await get_tree().create_timer(2.0).timeout
			break
	if m == null or m.scene_file_path != "res://game/ui/menu.tscn" or tl() == null:
		p("no menu or loader"); get_tree().quit(3); return
	var st: Dictionary = tl().get_status()
	var stages := []
	for x in tl().list_picker_packs("stage"): stages.append(String(x.get("id")))
	var balls := []
	for x in tl().list_picker_packs("ball"): balls.append(String(x.get("id")))
	p("packs=%d stages=%s balls=%s" % [int(st.get("packs", 0)), ",".join(stages), ",".join(balls)])
	p("security_refused=%d last=%s" % [int(st.get("security_refused", -1)), str(st.get("security_last_refusal", ""))])
	p("pwned=%d" % int(Engine.get_meta("jvm_pwned", 0)))
	var want := OS.get_environment("JV_PROBE_STAGE")
	if want == "": want = "1020"
	tl().set_picked_stage(want)
	var env := {}
	for i in 30:
		await get_tree().create_timer(0.5).timeout
		env = tl().get_env_status()
		if bool(env.get("applied", false)): break
	p("menu stage=%s env_applied=%s count=%d tris=%d" % [want, str(env.get("applied", false)), int(env.get("count", 0)), int(env.get("tris", 0))])
	st = tl().get_status()
	p("after security_refused=%d pwned=%d" % [int(st.get("security_refused", -1)), int(Engine.get_meta("jvm_pwned", 0))])
	p("done")
	get_tree().quit(0)
