extends Node




func p(msg: String) -> void: print("PROBE ", msg)
var out_dir := ""
func shot(name: String) -> void:
	for i in 12: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	var path := out_dir.path_join(name + ".png")
	img.save_png(path); p("screenshot=%s size=%s" % [path, img.get_size()])

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
	var tl: Node = get_node_or_null("/root/JVTextureLoader")
	if tl == null: p("no JVTextureLoader"); get_tree().quit(6); return
	for i in 10:
		await get_tree().create_timer(0.5).timeout
		var st0: Dictionary = tl.get_accessory_status() if tl.has_method("get_accessory_status") else {}
		if bool(st0.get("injected", false)):
			p("accessory_status=%s" % str(st0))
			break
		if i == 9: p("accessory_status=%s" % str(st0))
	var st: Dictionary = tl.get_accessory_status() if tl.has_method("get_accessory_status") else {}
	p("injected=%s catalog_added=%s entries=%s worn=%s" % [st.get("injected"), st.get("catalog_added"), str(st.get("entries", [])), str(st.get("worn", []))])
	var gc = get_node("/root/GameConfig")
	var ch = gc.character
	p("slots head=%s head2=%s face=%s extra=%s" % [ch.acc_id("head"), ch.acc_id("head2"), ch.acc_id("face"), ch.acc_id("extra")])
	var sync: Dictionary = tl.get_sync_status() if tl.has_method("get_sync_status") else {}
	p("equipped=%s" % str(sync.get("equipped", [])))
	gc.goto_scene("res://game/character/locker.tscn")
	var lk: Node = null
	for i in 40:
		await get_tree().create_timer(0.5).timeout
		lk = get_tree().current_scene
		if lk and lk.scene_file_path == "res://game/character/locker.tscn":
			var rail0: Dictionary = lk.get("_rail_btns") as Dictionary
			if rail0.has("accessories"): break
	if lk == null or lk.scene_file_path != "res://game/character/locker.tscn":
		p("locker not opened"); get_tree().quit(4); return
	var rail: Dictionary = lk.get("_rail_btns") as Dictionary
	p("locker=%s rail=%s" % [lk.get_path(), str(rail.keys())])
	if not rail.has("accessories"):
		p("no accessories rail"); get_tree().quit(5); return
	(rail["accessories"] as Button).pressed.emit()
	await get_tree().create_timer(2.0).timeout
	var fail := 0
	var want := {"head": "1002", "face": "10F0", "extra": "1003"}
	var labels_want := {"head": "Champion Cap", "face": "Custom Face", "extra": "Flame Tail"}
	for cat in ["head", "face", "extra"]:
		var subs: Dictionary = lk.get("_sub_btns") as Dictionary
		if not subs.has(cat):
			p("no sub tab %s subs=%s" % [cat, str(subs.keys())]); fail += 1; continue
		(subs[cat] as Button).pressed.emit()

		await get_tree().create_timer(3.0).timeout
		var acc_btns: Dictionary = lk.get("_acc_buttons") as Dictionary
		var ids: Array = []
		for k in acc_btns.keys(): ids.append(String(k))
		var catalog_ids: Array = []
		for it in AccessoryCatalog.for_category(cat, true):
			catalog_ids.append(it.id)
		var has := ids.has(want[cat]) or catalog_ids.has(want[cat])
		p("tab=%s section=%s sub=%s slot=%s worn=%s has_%s=%s buttons=%s" % [
			cat, lk.get("_section"), lk.get("_subsel").get("accessories", ""),
			lk.call("_acc_page_slot") if lk.has_method("_acc_page_slot") else "",
			ch.acc_id(cat), want[cat], has, str(ids)])
		if not has:
			fail += 1
			p("FAIL missing %s (%s) in %s" % [labels_want[cat], want[cat], cat])
		await shot("locker-accessories-%s" % cat)
	await get_tree().create_timer(0.5).timeout
	p("final_slots head=%s face=%s extra=%s equipped=%s fail=%d" % [
		ch.acc_id("head"), ch.acc_id("face"), ch.acc_id("extra"),
		str(tl.get_sync_status().get("equipped", [])), fail])
	p("done")
	get_tree().quit(1 if fail else 0)
