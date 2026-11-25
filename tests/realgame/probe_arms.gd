extends Node



func p(msg: String) -> void: print("PROBE ", msg)
var out_dir := ""
func shot(name: String) -> void:
	for i in 6: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image(); var path := out_dir.path_join(name + ".png"); img.save_png(path); p("screenshot=" + path)
func pose(pl: Node, rig: Node, clip: String, t: float) -> String:
	var anim: AnimationPlayer = rig.get("_anim")
	var found: String = rig.call("play_clip", clip, false)
	if found == "": return ""
	anim.speed_scale = 1.0; anim.play(found, 0.0); anim.seek(t * anim.get_animation(found).length, true); anim.speed_scale = 0.0
	(rig.get("_skel") as Skeleton3D).force_update_all_bone_transforms()
	var hl: Node3D = pl.get("hand_l"); var hr: Node3D = pl.get("hand_r")
	if hl and hr and rig.call("hands_animated"):
		hl.position = rig.call("hand_local", false); hr.position = rig.call("hand_local", true)
	return found
func _ready() -> void:
	out_dir = OS.get_environment("JV_PROBE_OUT")
	var m: Node = null
	for i in 150:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"): m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_card_btn") and m.get("player") != null:
			await get_tree().create_timer(2.0).timeout; break
	var sz := OS.get_environment("JV_PROBE_SIZE").split("x")
	var w := get_window(); w.mode = Window.MODE_WINDOWED; w.size = Vector2i(int(sz[0]), int(sz[1])); w.position = Vector2i.ZERO
	var pl: Node3D = m.get("player"); var rig: Node3D = pl.get("_model"); var sk: Skeleton3D = rig.get("_skel")
	var ml = get_node_or_null("/root/JVModelLoader")
	p("model=%s hand_radius=%.5f hand_rest=%s base_scale=%.5f bone_offset=%s bones=%s" % [ml.get_status().get("model") if ml else "-", rig.get("hand_radius"), rig.get("hand_rest"), rig.get("_base_scale"), str(rig.get("_bone_offset")), str(rig.get("_bone"))])
	var names := []
	for b in sk.get_bone_count(): names.append("%d:%s(p%d)" % [b, sk.get_bone_name(b), sk.get_bone_parent(b)])
	p("skeleton " + " ".join(names))
	var to_sk := sk.global_transform.affine_inverse() * rig.global_transform
	p("skel_scale=%s rig_to_skel=%s" % [sk.global_transform.basis.get_scale(), to_sk])
	pl.set_process(false); pl.set_physics_process(false)
	for c in pl.get_children():
		if not (c is Node3D):
			c.set_process(false); c.set_physics_process(false)
	m.set_process(false); m.set_physics_process(false)
	var ui = m.get("ui"); if ui: ui.visible = false
	var cam := Camera3D.new(); cam.fov = 38.0; m.add_child(cam)
	var fwd := -pl.global_transform.basis.z.normalized(); var side := pl.global_transform.basis.x.normalized()
	var target := pl.global_position + Vector3(0, 0.9, 0)
	var tag := OS.get_environment("JV_TAG")

	var fixed := {"ready_bump": [pl.get("POSE_BUMP_L"), pl.get("POSE_BUMP_R")], "ready_set": [pl.get("POSE_SET_L"), pl.get("POSE_SET_R")], "serve_hold": [pl.get("POSE_SERVE_L"), pl.get("POSE_SERVE_R")]}
	for clip in [["idle", 0.3], ["recieve", 0.5], ["set", 0.5], ["spike", 0.45], ["dive", 0.5], ["ready_bump", 0.3], ["ready_set", 0.3], ["serve_hold", 0.3]]:
		var found := pose(pl, rig, "idle" if fixed.has(clip[0]) else clip[0], clip[1])
		if fixed.has(clip[0]):
			var hl: Node3D = pl.get("hand_l"); var hr: Node3D = pl.get("hand_r")
			hl.position = fixed[clip[0]][0]; hr.position = fixed[clip[0]][1]
		for i in 3: await get_tree().process_frame
		var arms := sk.find_children("JvmArm*", "MeshInstance3D", false, false)
		var hands := [pl.get("hand_l"), pl.get("hand_r")]
		var wr := []; var el := []
		for a in arms:
			if a.name.begins_with("JvmArmWrist"): wr.append(a)
			if a.name.begins_with("JvmArmElbow"): el.append(a)
		var gaps := []
		for h in hands:
			var best := 1e9
			for wn in wr: best = minf(best, (wn as Node3D).global_position.distance_to((h as Node3D).global_position))
			gaps.append("%.4f" % best)
		p("arms pose=%s nodes=%d visible=%s wrist_to_hand_gap_m=%s elbows=%s hands=%s status=%s" % [clip[0], arms.size(), str(arms.map(func(a): return a.visible).count(true)), str(gaps), str(el.map(func(e): return e.global_position)), str(hands.map(func(h): return h.global_position)), str(ml.get_status().get("arms") if ml else "-")])
		for s in ["l", "r"]:
			var hb: int = (rig.get("_bone") as Dictionary)[s]
			var fa := sk.get_bone_parent(hb); var ua := sk.get_bone_parent(fa)
			var sphere: Vector3 = to_sk * (rig.call("hand_local", s == "r") as Vector3)
			var hand_o := sk.get_bone_global_pose(hb).origin
			p("pose=%s clip=%s side=%s upper(%s)=%s fore(%s)=%s hand(%s)=%s sphere=%s gap=%.3f hand_basis=%s" % [clip[0], found, s, sk.get_bone_name(ua), sk.get_bone_global_pose(ua).origin, sk.get_bone_name(fa), sk.get_bone_global_pose(fa).origin, sk.get_bone_name(hb), hand_o, sphere, hand_o.distance_to(sphere), sk.get_bone_global_pose(hb).basis])
		cam.global_position = target - fwd * -3.0 + Vector3(0, 0.25, 0); cam.look_at(target); cam.current = true
		await shot("%sarms-%s-front" % [tag, clip[0]])
		cam.global_position = target + side * 3.0 + Vector3(0, 0.25, 0); cam.look_at(target)
		await shot("%sarms-%s-side" % [tag, clip[0]])
	p("done"); get_tree().quit(0)
