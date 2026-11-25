extends Node




func p(msg: String) -> void: print("PROBE ", msg)
var m: Node = null
var out_dir := ""
func shot(name: String) -> void:
	for i in 8: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	var path := out_dir.path_join(name + ".png")
	img.save_png(path); p("screenshot=%s" % path)
func frames(n := 10) -> void:
	for i in n: await get_tree().process_frame
func grafts(rig: Node) -> int:
	var n := 0
	var sk: Node = rig.get("_skel")
	if sk: for c in sk.get_children(): if c.has_meta("jvm_model"): n += 1
	return n
func named(root: Node, prefix: String) -> Array:
	var out := []
	for c in root.find_children(prefix + "*", "", true, false): out.append(c)
	return out
func _cp_dir(from: String, to: String) -> void:
	DirAccess.make_dir_recursive_absolute(to)
	for f in DirAccess.get_files_at(from): DirAccess.copy_absolute(from.path_join(f), to.path_join(f))
func _rm_dir(path: String) -> void:
	for f in DirAccess.get_files_at(path): DirAccess.remove_absolute(path.path_join(f))
	DirAccess.remove_absolute(path)
func _wjson(path: String, v) -> void:
	var f := FileAccess.open(path, FileAccess.WRITE); f.store_string(JSON.stringify(v)); f.close()
func ball_info(ball: Node) -> String:
	var phys = ball.get("PHYS")
	var r: float = phys.radius if phys else -1.0
	var vis := []
	var mats := []
	for mi in ball.find_children("*", "MeshInstance3D", true, false):
		var n := String(mi.name).to_lower()
		if n.contains("shadow") or n.contains("outline") or n.contains("ring") or n.contains("column"): continue
		if (mi as MeshInstance3D).mesh == null: continue
		var aabb: AABB = (mi as MeshInstance3D).get_aabb()
		var sc: Vector3 = (mi as Node3D).global_transform.basis.get_scale()
		vis.append("%s:%.3f" % [mi.name, aabb.size.x * 0.5 * sc.x])
		var mat: Material = (mi as MeshInstance3D).material_override
		if mat == null and (mi as MeshInstance3D).mesh.get_surface_count() > 0: mat = (mi as MeshInstance3D).get_active_material(0)
		if mat is ShaderMaterial: mats.append("%s color_a=%s color_b=%s tex=%s" % [mi.name, mat.get_shader_parameter("color_a"), mat.get_shader_parameter("color_b"), mat.get_shader_parameter("albedo_tex")])
	var cols := []
	for cs in ball.find_children("*", "CollisionShape3D", true, false):
		if (cs as CollisionShape3D).shape is SphereShape3D: cols.append("%.3f" % ((cs as CollisionShape3D).shape as SphereShape3D).radius)
	return "phys_radius=%.3f visual_radius=%s collision=%s mats=%s" % [r, vis, cols, mats]
func report(tag: String, tl: Node, ml: Node, pl: Node, rig: Node) -> void:
	var ts: Dictionary = tl.get_status() if tl else {}
	var ms: Dictionary = ml.get_status() if ml else {}
	p("%s textures packs=%s equipped=%s spawned=%s swaps=%s warnings=%s" % [tag, ts.get("packs"), str(tl.get_sync_status().get("equipped")) if tl else "-", ts.get("spawned"), ts.get("global_swaps"), ts.get("warnings")])
	p("%s models state=%s packs=%s model=%s grafts=%d last_error=%s" % [tag, ms.get("state"), ms.get("packs"), ms.get("model"), grafts(rig), ms.get("last_error", "")])
	p("%s accessories hat=%d back=%d mounts=%d springbones=%d" % [tag, named(pl, "JVM_hat").size() + named(pl, "JVM_Hat").size(), named(pl, "JVM_back").size() + named(pl, "JVM_Back").size(), named(pl, "JVM_Mount_").size(), named(pl, "JVM_SpringBones").size()])
	var names := []
	for c in named(pl, "JVM_"): names.append(String(c.name))
	p("%s jvm_nodes=%s" % [tag, names])
	var jm = (rig.get("_mats") as Dictionary).get("jersey")
	p("%s jersey_tex=%s albedo=%s" % [tag, jm.get_shader_parameter("albedo_tex") if jm is ShaderMaterial else "?", jm.get_shader_parameter("albedo") if jm is ShaderMaterial else "?"])
	var b = m.get("ball")
	if b: p("%s ball %s" % [tag, ball_info(b)])

func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
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
	var rig: Node = pl.get("_model")
	p("loaders textures=%s models=%s" % [tl != null, ml != null])
	var M := ProjectSettings.globalize_path("res://mods")
	var MD := M.path_join("models"); var T := M.path_join("resources/packs")
	p("models_dir=%s" % [DirAccess.get_directories_at(MD)])
	p("packs_dir=%s" % [DirAccess.get_directories_at(T)])
	await frames(20)
	report("installed", tl, ml, pl, rig)
	var ui = m.get("ui")
	pl.set_process(false); pl.set_physics_process(false); m.set_process(false); m.set_physics_process(false)
	await shot("01-menu_view_installed")
	var cam := Camera3D.new(); cam.fov = 38.0; m.add_child(cam)
	if ui: ui.visible = false
	var fwd := -pl.global_transform.basis.z.normalized()
	var side := pl.global_transform.basis.x.normalized()
	var target := pl.global_position + Vector3(0, 0.95, 0)
	for v in [["02-front", fwd * -3.4 + Vector3(0, 0.3, 0)], ["03-three_quarter", side * 2.6 - fwd * 2.2 + Vector3(0, 0.4, 0)], ["04-back", fwd * 3.4 + Vector3(0, 0.5, 0)], ["05-side", side * 3.4 + Vector3(0, 0.3, 0)]]:
		cam.global_position = target + v[1]; cam.look_at(target); cam.current = true
		await shot(v[0])
	var b: Node3D = m.get("ball")
	if b:
		b.visible = true
		cam.global_position = b.global_position + Vector3(0.0, 0.25, 1.2); cam.look_at(b.global_position); await shot("06-ball_close")

	var ball_with := ball_info(b).split(" mats=")[0] if b else ""
	DirAccess.rename_absolute(T.path_join("06_galaxy_ball"), M.path_join("_off_06")); tl.call("reload_config"); await frames()
	report("no06", tl, ml, pl, rig)
	if b: await shot("07-ball_close_vanilla")
	DirAccess.rename_absolute(M.path_join("_off_06"), T.path_join("06_galaxy_ball")); tl.call("reload_config"); await frames()
	p("ball_radius_same=%s (%s)" % [ball_info(b).split(" mats=")[0] == ball_with if b else "no ball", ball_with])

	cam.global_position = target + side * 2.6 - fwd * 2.2 + Vector3(0, 0.4, 0); cam.look_at(target)
	DirAccess.rename_absolute(T.path_join("03_flame_tail_back"), M.path_join("_off_03"))
	_cp_dir(T.path_join("02_champion_cap_hat"), T.path_join("07_cap_copy_renamed"))
	var sj = JSON.parse_string(FileAccess.get_file_as_string(T.path_join("07_cap_copy_renamed/skin.json")))
	sj["id"] = "1007"; sj["name"] = "Cap copy"; _wjson(T.path_join("07_cap_copy_renamed/skin.json"), sj)
	_wjson(T.path_join("equipped.json"), {"equipped": ["1001", "1007", "1003"]}); tl.call("reload_config"); await frames()
	report("swap03for07", tl, ml, pl, rig)
	await shot("08-packs_03_removed_07_added")
	_rm_dir(T.path_join("07_cap_copy_renamed"))
	DirAccess.rename_absolute(M.path_join("_off_03"), T.path_join("03_flame_tail_back"))
	_wjson(T.path_join("equipped.json"), {"equipped": ["1001", "1002", "1003"]}); tl.call("reload_config"); await frames()
	report("restored_packs", tl, ml, pl, rig)

	_cp_dir(MD.path_join("plump_bean"), MD.path_join("bean_copy_renamed"))
	var mj = JSON.parse_string(FileAccess.get_file_as_string(MD.path_join("bean_copy_renamed/skin.json")))
	mj["id"] = "BEA1"; mj["name"] = "Bean copy"; _wjson(MD.path_join("bean_copy_renamed/skin.json"), mj)
	_wjson(MD.path_join("equipped.json"), {"model": "BEA1"}); ml.call("reload_config"); await frames()
	report("model_copy", tl, ml, pl, rig)
	_rm_dir(MD.path_join("bean_copy_renamed")); ml.call("reload_config"); await frames()
	report("model_copy_deleted", tl, ml, pl, rig)
	_wjson(MD.path_join("equipped.json"), {"model": "BEA0"}); ml.call("reload_config"); await frames()
	DirAccess.rename_absolute(M.path_join("resources"), M.path_join("_off_resources")); tl.call("reload_config"); await frames()
	report("no_resources_folder", tl, ml, pl, rig)
	await shot("09-no_resources_folder")
	DirAccess.rename_absolute(M.path_join("_off_resources"), M.path_join("resources")); tl.call("reload_config")
	DirAccess.rename_absolute(MD, M.path_join("_off_models")); ml.call("reload_config"); await frames()
	report("no_models_folder", tl, ml, pl, rig)
	await shot("10-no_models_folder")
	DirAccess.rename_absolute(M.path_join("_off_models"), MD); ml.call("reload_config"); await frames()
	report("final", tl, ml, pl, rig)
	await shot("11-final")
	p("done")
	get_tree().quit(0)
