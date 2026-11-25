extends Node


func p(msg: String) -> void: print("PROBE ", msg)
var m: Node = null
var out_dir := ""
var prefix := "before"

func shot(name: String) -> void:
	for i in 8: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	var path := out_dir.path_join(prefix + "-" + name + ".png")
	img.save_png(path); p("screenshot=%s" % path)

func frames(n := 12) -> void:
	for i in n: await get_tree().process_frame

func named(root: Node, needle: String) -> Array:
	var out := []
	if root == null: return out
	for c in root.find_children("*", "", true, false):
		if String(c.name).to_lower().contains(needle.to_lower()): out.append(c)
	return out

func dump_xf(tag: String, n: Node) -> void:
	if n == null:
		p("%s missing" % tag); return
	if n is Node3D:
		var n3 := n as Node3D
		var t := n3.global_transform
		var o := t.origin
		var aabb_s := ""
		if n is MeshInstance3D and (n as MeshInstance3D).mesh:
			var ab: AABB = (n as MeshInstance3D).get_aabb()
			var g: AABB = n3.global_transform * ab
			aabb_s = " aabb_global=(%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f)" % [g.position.x, g.position.y, g.position.z, g.position.x+g.size.x, g.position.y+g.size.y, g.position.z+g.size.z]
		p("%s name=%s pos=(%.4f,%.4f,%.4f) scale=%s%s" % [tag, n.name, o.x, o.y, o.z, n3.scale, aabb_s])
	else:
		p("%s name=%s class=%s" % [tag, n.name, n.get_class()])

func dump_tree(tag: String, n: Node, depth := 0) -> void:
	if n == null or depth > 6: return
	if n is Node3D:
		dump_xf("%s%s" % [tag, "  ".repeat(depth)], n)
	for c in n.get_children():
		dump_tree(tag, c, depth + 1)

func head_pos(pl: Node3D) -> Vector3:
	var rig = pl.get("_model")
	if rig:
		var sk = rig.get("_skel")
		if sk and sk is Skeleton3D:
			var i: int = (sk as Skeleton3D).find_bone("head")
			if i >= 0:



				var xf := (sk as Skeleton3D).global_transform * (sk as Skeleton3D).get_bone_global_pose(i)
				return xf.origin + xf.basis.y.normalized() * 0.45
	return pl.global_position + Vector3(0, 1.55, 0)

func report(tag: String, pl: Node, tl: Node, ml: Node) -> void:
	var ts: Dictionary = tl.get_status() if tl else {}
	var ms: Dictionary = ml.get_status() if ml else {}
	p("%s textures equipped=%s spawned=%s" % [tag, str(tl.get_sync_status().get("equipped")) if tl else "-", ts.get("spawned")])
	p("%s models state=%s model=%s shown=%s saved=%s applied=%s" % [tag, ms.get("state"), ms.get("equipped"), ml.get_shown_model() if ml else "?", ml.get_saved_model() if ml else "?", ms.get("applied")])
	var hats := named(pl, "JVM_Hat") + named(pl, "JVM_hat")
	var mounts := named(pl, "JVM_Mount_")
	p("%s hat_nodes=%d mounts=%d" % [tag, hats.size(), mounts.size()])
	for n in mounts: dump_xf("%s mount" % tag, n)
	for n in hats:
		dump_xf("%s hat_root" % tag, n)
		dump_tree("%s hat " % tag, n)

	var hp := head_pos(pl)
	p("%s head_pos=(%.4f,%.4f,%.4f)" % [tag, hp.x, hp.y, hp.z])

func views(pl: Node3D, cam: Camera3D, label: String) -> void:
	var fwd := -pl.global_transform.basis.z.normalized()
	var side := pl.global_transform.basis.x.normalized()
	var body := pl.global_position + Vector3(0, 0.95, 0)
	var head := head_pos(pl)
	var body_cams := [
		["%s-front" % label, body, fwd * -3.4 + Vector3(0, 0.3, 0)],
		["%s-three_quarter" % label, body, side * 2.6 - fwd * 2.2 + Vector3(0, 0.4, 0)],
		["%s-back" % label, body, fwd * 3.4 + Vector3(0, 0.5, 0)],
		["%s-side" % label, body, side * 3.4 + Vector3(0, 0.3, 0)],
	]
	for v in body_cams:
		cam.global_position = v[1] + v[2]; cam.look_at(v[1]); cam.current = true
		await shot(v[0])
	var head_cams := [
		["%s-head-front" % label, head, fwd * -1.35 + Vector3(0, 0.05, 0)],
		["%s-head-three_quarter" % label, head, side * 1.05 - fwd * 0.95 + Vector3(0, 0.08, 0)],
		["%s-head-back" % label, head, fwd * 1.35 + Vector3(0, 0.12, 0)],
		["%s-head-side" % label, head, side * 1.35 + Vector3(0, 0.05, 0)],
	]
	for v in head_cams:
		cam.global_position = v[1] + v[2]; cam.look_at(v[1]); cam.current = true
		await shot(v[0])

func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
	prefix = OS.get_environment("JV_PROBE_PREFIX")
	if prefix == "": prefix = "shot"
	for i in 150:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"):
			p("boot scene -> continue"); m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			p("title -> menu"); get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_card_btn") and m.get("player") != null:
			await get_tree().create_timer(3.0).timeout
			break
	if m == null or m.get("player") == null: p("no menu player"); get_tree().quit(3); return
	var sz := OS.get_environment("JV_PROBE_SIZE").split("x")
	var w := get_window(); w.mode = Window.MODE_WINDOWED; w.size = Vector2i(int(sz[0]), int(sz[1])); w.position = Vector2i.ZERO
	var tl: Node = get_node_or_null("/root/JVTextureLoader")
	var ml: Node = get_node_or_null("/root/JVModelLoader")
	var pl: Node3D = m.get("player")
	p("loaders textures=%s models=%s prefix=%s" % [tl != null, ml != null, prefix])
	await frames(30)
	pl.set_process(false); pl.set_physics_process(false); m.set_process(false); m.set_physics_process(false)
	var ui = m.get("ui")
	if ui: ui.visible = false
	var cam := Camera3D.new(); cam.fov = 38.0; m.add_child(cam)

	report("start", pl, tl, ml)
	await views(pl, cam, "bean")

	var M := ProjectSettings.globalize_path("res://mods")
	var MD := M.path_join("models")
	var eq_path := MD.path_join("equipped.json")
	var prev := FileAccess.get_file_as_string(eq_path)
	var f := FileAccess.open(eq_path, FileAccess.WRITE); f.store_string("{\"model\": \"\"}"); f.close()
	if ml: ml.call("reload_config")
	await frames(40)
	if tl: tl.call("reload_config")
	await frames(40)
	report("vanilla", pl, tl, ml)
	await views(pl, cam, "vanilla")

	f = FileAccess.open(eq_path, FileAccess.WRITE); f.store_string(prev if prev != "" else "{\"model\": \"BEA0\"}"); f.close()
	if ml: ml.call("reload_config")
	await frames(20)
	p("done")
	get_tree().quit(0)
