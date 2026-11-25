extends Node



func p(msg: String) -> void: print("PROBE ", msg)
var out_dir := ""
var verdicts: Array = []
func verdict(name: String, ok: bool, detail: String) -> void:
	verdicts.append([name, ok]); p("VERDICT %s %s %s" % [name, "PASS" if ok else "FAIL", detail])
func shot(name: String) -> void:
	for i in 8: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image(); var path := out_dir.path_join(name + ".png"); img.save_png(path); p("screenshot=" + path)
func player_of(n: Node) -> Node:
	var a := n
	while a and not (a.get_script() and String(a.get_script().resource_path).ends_with("player/player.gd")): a = a.get_parent()
	return a

func same_look(a, b) -> bool:
	if a == null or b == null: return false
	if a == b: return true
	if not (a.has_method("look_dict") and b.has_method("look_dict")): return false
	var da: Dictionary = a.look_dict().duplicate(); var db: Dictionary = b.look_dict().duplicate()
	da.erase("h"); db.erase("h")
	return str(da) == str(db)
func census(tag: String, local: Node) -> Dictionary:
	var out := {"mine": 0, "others": 0, "others_grafted": 0, "mine_grafted": 0, "list": []}
	for n in get_tree().root.find_children("*", "Node3D", true, false):
		if not (n.get_script() and String(n.get_script().resource_path).ends_with("character/model_rig.gd")): continue
		if not n.is_visible_in_tree(): continue
		var grafted := n.find_children("JvmModel*", "", true, false).size() > 0
		var pl := player_of(n)


		var gcc = get_node("/root/GameConfig").get("character")
		var is_mine: bool = pl != null and (pl == local or same_look(gcc, pl.get("character")))
		out["mine" if is_mine else "others"] += 1
		if grafted: out["mine_grafted" if is_mine else "others_grafted"] += 1
		out["list"].append("%s%s %s" % ["*" if is_mine else " ", "BEAN/MODEL" if grafted else "game model", str(n.get_path()).right(90)])
	p("census %s: mine=%d (grafted %d) others=%d (grafted %d)" % [tag, out["mine"], out["mine_grafted"], out["others"], out["others_grafted"]])
	for l in out["list"]: p("   " + l)
	return out
func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
	var ml = get_node_or_null("/root/JVModelLoader")
	var m: Node = null
	var title_done := false
	for i in 150:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"): m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			if not title_done:
				await get_tree().create_timer(6.0).timeout
				var c := census("title", null)
				verdict("title_crowd", c["others"] > 0 and c["others_grafted"] == 0 and c["mine_grafted"] == c["mine"], "crowd: %d other characters, %d grafted; your own lead figure: %d/%d grafted" % [c["others"], c["others_grafted"], c["mine_grafted"], c["mine"]])
				await shot("title"); title_done = true
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_try_freeplay") and m.get("player") != null:
			await get_tree().create_timer(4.0).timeout; break
	var c2 := census("menu", m.get("player"))
	verdict("menu", c2["mine_grafted"] >= 1, "menu player grafted %d, other rigs grafted %d (menu cards: grafted only when they wear your look)" % [c2["mine_grafted"], c2["others_grafted"]])
	await shot("menu")

	var gc = get_node("/root/GameConfig")
	gc.set("cpu_fill", 0)
	if gc.get("match_intro") != null: gc.set("match_intro", false)
	m.call("_go_to_match", 2)
	var main: Node = null
	for i in 60:
		await get_tree().create_timer(1.0).timeout
		var s := get_tree().current_scene
		if s and s.scene_file_path == "res://game/main/main.tscn" and s.get("player") != null: main = s; break
	if main == null: p("no match scene"); verdict("match", false, "match did not start"); get_tree().quit(1); return
	await get_tree().create_timer(8.0).timeout
	ml = get_node_or_null("/root/JVModelLoader")
	p("gate mode=%s models=%s" % [get_node("/root/JVModGate").get_mode() if get_node_or_null("/root/JVModGate") else "?", ml.get_status().get("model") if ml else "?"])
	var c3 := census("match BEA0", main.get("player"))
	verdict("match_bean", c3["mine_grafted"] == c3["mine"] and c3["mine"] >= 1 and c3["others"] >= 1 and c3["others_grafted"] == 0, "you: %d/%d grafted, CPU/other players: %d rigs, %d grafted" % [c3["mine_grafted"], c3["mine"], c3["others"], c3["others_grafted"]])
	await shot("match-bean")

	ml = get_node_or_null("/root/JVModelLoader")
	var ok: bool = ml != null and bool(ml.call("apply_model", "7E51"))
	await get_tree().create_timer(3.0).timeout
	var c4 := census("match 7E51", main.get("player"))
	verdict("match_ts01", ok and c4["mine_grafted"] == c4["mine"] and c4["mine"] >= 1 and c4["others_grafted"] == 0, "apply_model(7E51)=%s; you: %d/%d grafted, others %d grafted; status %s" % [ok, c4["mine_grafted"], c4["mine"], c4["others_grafted"], ml.get_status().get("model")])
	await shot("match-ts01")
	ml.call("apply_model", "BEA0")
	var failed := verdicts.filter(func(v): return not v[1]).map(func(v): return v[0])
	p("RESULT %s passed=%d failed=%s" % ["ALL PASS" if failed.is_empty() else "FAIL", verdicts.size() - failed.size(), str(failed)])
	get_tree().quit(0 if failed.is_empty() else 1)
