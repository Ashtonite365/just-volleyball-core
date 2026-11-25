extends Node


func p(msg: String) -> void: print("PROBE ", msg)

func tl() -> Node: return get_node_or_null("/root/JVTextureLoader")

func snap(tag: String) -> Dictionary:
	var n := tl()
	var st := {}
	if n:
		st = n.get_status()
		st["cache_ids"] = n.get_cache_ids()
		st["disk_loads"] = n.get_disk_load_count()
		st["cache"] = n.get_cache_count()
		st["swaps"] = n.get_swapped_count()
		st["overrides"] = n.get_override_count()
	p("%s scene=%s disk_loads=%s cache=%s swaps=%s overrides=%s packs=%s picked_stage=%s picked_ball=%s gate=%s" % [
		tag,
		str(get_tree().current_scene.scene_file_path if get_tree().current_scene else "?"),
		str(st.get("disk_loads", -1)), str(st.get("cache", -1)),
		str(st.get("swaps", -1)), str(st.get("overrides", -1)),
		str(st.get("packs", -1)), str(st.get("picked_stage", "")), str(st.get("picked_ball", "")),
		str(st.get("state", ""))
	])
	return st

func same_ids(a: Dictionary, b: Dictionary) -> bool:
	var ids_a: Dictionary = a.get("cache_ids", {})
	var ids_b: Dictionary = b.get("cache_ids", {})
	for k in ids_a.keys():
		if int(ids_a[k]) != int(ids_b.get(k, 0)):
			return false
	return true

func _ready() -> void:
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
	if m == null or m.scene_file_path != "res://game/ui/menu.tscn":
		p("no menu"); get_tree().quit(3); return
	if tl() == null:
		p("no JVTextureLoader"); get_tree().quit(4); return

	for i in 20:
		await get_tree().create_timer(0.25).timeout
		if tl().get_cache_count() > 0: break
	var menu0 := snap("menu-before-locker")
	p("menu_cache_ready=%s" % [int(menu0.get("cache", 0)) > 0])
	get_node("/root/GameConfig").goto_scene("res://game/character/locker.tscn")
	var lk: Node = null
	for i in 40:
		await get_tree().create_timer(0.5).timeout
		lk = get_tree().current_scene
		if lk and lk.scene_file_path == "res://game/character/locker.tscn" and lk.is_node_ready():
			break
	if lk == null or lk.scene_file_path != "res://game/character/locker.tscn":
		p("locker not opened"); get_tree().quit(5); return
	await get_tree().create_timer(1.0).timeout
	var locker := snap("in-locker")
	p("locker_no_reload=%s" % [int(locker.get("disk_loads", -1)) == int(menu0.get("disk_loads", -2))])
	p("locker_cache_same=%s" % [same_ids(menu0, locker) and int(locker.get("cache", 0)) >= int(menu0.get("cache", 0))])

	get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn")
	var menu: Node = null
	for i in 40:
		await get_tree().create_timer(0.5).timeout
		menu = get_tree().current_scene
		if menu and menu.scene_file_path == "res://game/ui/menu.tscn" and menu.is_node_ready():
			break
	if menu == null or menu.scene_file_path != "res://game/ui/menu.tscn":
		p("menu not restored"); get_tree().quit(6); return
	await get_tree().create_timer(1.5).timeout
	var after := snap("menu-after-locker")
	p("after_locker_no_reload=%s" % [int(after.get("disk_loads", -1)) == int(menu0.get("disk_loads", -2))])
	p("after_locker_cache_same=%s" % [same_ids(menu0, after)])
	p("after_locker_still_loaded=%s" % [int(after.get("cache", 0)) >= int(menu0.get("cache", 0)) and String(after.get("state", "")) == "active"])

	if menu.has_method("_enter_freeplay"):
		menu.call("_enter_freeplay")
		await get_tree().create_timer(2.0).timeout
		var fp := snap("freeplay")
		p("freeplay_no_reload=%s" % [int(fp.get("disk_loads", -1)) == int(menu0.get("disk_loads", -2))])
		p("freeplay_cache_same=%s" % [same_ids(menu0, fp)])
		p("freeplay_reapplied=%s" % [String(fp.get("state", "")) == "active"])
	else:
		p("freeplay_skipped=no _enter_freeplay")
	p("done")
	get_tree().quit(0)
