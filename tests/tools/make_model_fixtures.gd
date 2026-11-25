extends SceneTree




const NAMES := ["root", "heelik.l", "foot.l", "toe.l", "heelik.r", "foot.r", "toe.r", "kneeik.l", "kneeik.r", "spine",
	"hip.l", "thigh.l", "calf.l", "hip.r", "thigh.r", "calf.r", "spine1", "head", "shoulder.l", "arm.l", "forearm.l", "hand.l",
	"shoulder.r", "arm.r", "forearm.r", "hand.r"]
const PARENT := [-1, 0, 1, 2, 0, 4, 5, 0, 0, 0, 9, 10, 11, 9, 13, 14, 9, 16, 16, 18, 19, 20, 16, 22, 23, 24]
const OFF := [Vector3(0, 0, 0), Vector3(0.25, 0.1, 0), Vector3(0, 0.12, 0), Vector3(0, -0.1, 0.2), Vector3(-0.25, 0.1, 0), Vector3(0, 0.12, 0), Vector3(0, -0.1, 0.2),
	Vector3(0.25, 1.0, 0.6), Vector3(-0.25, 1.0, 0.6), Vector3(0, 1.15, 0), Vector3(0.25, 0, 0), Vector3(0, -0.05, 0), Vector3(0, -0.5, 0),
	Vector3(-0.25, 0, 0), Vector3(0, -0.05, 0), Vector3(0, -0.5, 0), Vector3(0, 0.65, 0), Vector3(0, 0.7, 0), Vector3(0, 0.7, 0),
	Vector3(0.2, -0.07, 0), Vector3(0.2, -0.27, 0), Vector3(0.38, -0.6, 0), Vector3(0, 0.7, 0), Vector3(-0.2, -0.07, 0), Vector3(-0.2, -0.27, 0), Vector3(-0.38, -0.6, 0)]

func skel(names := NAMES) -> Skeleton3D:
	var s := Skeleton3D.new(); s.name = "Skeleton3D"
	for i in names.size():
		s.add_bone(names[i])
		if PARENT[i] >= 0: s.set_bone_parent(i, PARENT[i])
		s.set_bone_rest(i, Transform3D(Basis(), OFF[i])); s.set_bone_pose_position(i, OFF[i])
	return s

func grest(s: Skeleton3D, i: int) -> Vector3: return s.get_bone_global_rest(i).origin

func owned(n: Node, owner: Node) -> void:
	for c in n.get_children():
		c.owner = owner; owned(c, owner)

func mat(nm: String, c: Color) -> StandardMaterial3D:
	var m := StandardMaterial3D.new(); m.resource_name = nm; m.albedo_color = c; return m

func base_scene() -> Node3D:
	var root := Node3D.new(); root.name = "player_char"
	var arm := Node3D.new(); arm.name = "Armature"; root.add_child(arm)
	var s := skel(); arm.add_child(s)
	var add := func(nm: String, mesh: Mesh, pos: Vector3, m: Material):
		var mi := MeshInstance3D.new(); mi.name = nm; mesh.surface_set_material(0, m) if mesh is ArrayMesh else mesh.set("material", m)
		mi.mesh = mesh; mi.position = pos; s.add_child(mi)
	var b := BoxMesh.new(); b.size = Vector3(0.8, 2.0, 0.5); add.call("bodymesh", b, Vector3(0, 1.6, 0), mat("skin", Color(0.9, 0.9, 0.9)))
	var j := BoxMesh.new(); j.size = Vector3(0.85, 1.0, 0.55); add.call("jerseymesh", j, Vector3(0, 2.1, 0), mat("jersey", Color(0.9, 0.9, 0.9)))
	var h := SphereMesh.new(); h.radius = 0.6; h.height = 1.2; add.call("headmesh", h, Vector3(0, 3.3, 0), mat("skin", Color(0.9, 0.9, 0.9)))
	var hb := BoxMesh.new(); hb.size = Vector3(0.2, 0.2, 0.28); add.call("handsmesh", hb, grest(s, 21), mat("skin", Color(0.9, 0.9, 0.9)))
	var ap := AnimationPlayer.new(); ap.name = "AnimationPlayer"; root.add_child(ap)
	var lib := AnimationLibrary.new()
	for clip in [["idle", 0.1], ["spike", 2.4], ["dive", -1.2]]:
		var a := Animation.new(); a.length = 1.0
		for bone in ["arm.r", "forearm.r", "spine1"]:
			var t := a.add_track(Animation.TYPE_ROTATION_3D); a.track_set_path(t, "Armature/Skeleton3D:" + bone)
			a.rotation_track_insert_key(t, 0.0, Quaternion())
			a.rotation_track_insert_key(t, 0.5, Quaternion(Vector3(0, 0, 1), clip[1] * (1.0 if bone != "spine1" else 0.2)))
			a.rotation_track_insert_key(t, 1.0, Quaternion(Vector3(0, 0, 1), clip[1] * (1.0 if bone != "spine1" else 0.2)))
		lib.add_animation(clip[0], a)
	ap.add_animation_library("", lib)
	owned(root, root)
	return root

func box_arrays(size: Vector3, center: Vector3, bone: int) -> Array:
	var bm := BoxMesh.new(); bm.size = size
	var a := bm.get_mesh_arrays()
	var v: PackedVector3Array = a[Mesh.ARRAY_VERTEX]
	for i in v.size(): v[i] += center
	a[Mesh.ARRAY_VERTEX] = v
	var bones := PackedInt32Array(); var w := PackedFloat32Array()
	for i in v.size():
		bones.append_array([bone, 0, 0, 0]); w.append_array([1.0, 0.0, 0.0, 0.0])
	a[Mesh.ARRAY_BONES] = bones; a[Mesh.ARRAY_WEIGHTS] = w
	a[Mesh.ARRAY_TANGENT] = null
	return a

func merge(parts: Array) -> Array:
	var out := []; out.resize(Mesh.ARRAY_MAX)
	var v := PackedVector3Array(); var n := PackedVector3Array(); var uv := PackedVector2Array(); var b := PackedInt32Array(); var w := PackedFloat32Array(); var idx := PackedInt32Array()
	for p in parts:
		var base := v.size()
		v.append_array(p[Mesh.ARRAY_VERTEX]); n.append_array(p[Mesh.ARRAY_NORMAL]); uv.append_array(p[Mesh.ARRAY_TEX_UV])
		b.append_array(p[Mesh.ARRAY_BONES]); w.append_array(p[Mesh.ARRAY_WEIGHTS])
		for i in p[Mesh.ARRAY_INDEX]: idx.append(i + base)
	out[Mesh.ARRAY_VERTEX] = v; out[Mesh.ARRAY_NORMAL] = n; out[Mesh.ARRAY_TEX_UV] = uv; out[Mesh.ARRAY_BONES] = b; out[Mesh.ARRAY_WEIGHTS] = w; out[Mesh.ARRAY_INDEX] = idx
	return out

func bean(names := NAMES, scripted := false) -> Node3D:
	var root := Node3D.new(); root.name = "bean"
	var arm := Node3D.new(); arm.name = "Armature"; root.add_child(arm)
	var s := skel(names); arm.add_child(s)
	var parts := [box_arrays(Vector3(1.0, 1.0, 1.0), grest(s, 17) + Vector3(0, 0.6, 0), 17), box_arrays(Vector3(0.7, 1.2, 0.45), grest(s, 16) + Vector3(0, 0.1, 0), 16)]
	for side in [[19, 20], [23, 24]]:
		parts.append(box_arrays(Vector3(0.2, 0.2, 0.2), (grest(s, side[0]) + grest(s, side[1])) * 0.5, side[0]))
		parts.append(box_arrays(Vector3(0.18, 0.18, 0.18), (grest(s, side[1]) + grest(s, side[1] + 1)) * 0.5, side[1]))
	for leg in [[11, 12], [14, 15]]:
		parts.append(box_arrays(Vector3(0.25, 0.5, 0.25), grest(s, leg[0]) + Vector3(0, -0.25, 0), leg[0]))
		parts.append(box_arrays(Vector3(0.22, 0.5, 0.22), grest(s, leg[1]) + Vector3(0, -0.25, 0), leg[1]))
	var am := ArrayMesh.new()
	am.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, merge(parts))
	am.surface_set_material(0, mat("bean_mat", Color(0.1, 0.8, 0.9)))
	var skin := Skin.new()
	for i in s.get_bone_count(): skin.add_bind(i, s.get_bone_global_rest(i).affine_inverse())
	var mi := MeshInstance3D.new(); mi.name = "beanmesh"; mi.mesh = am; mi.skin = skin
	s.add_child(mi); mi.skeleton = NodePath("..")
	owned(root, root)
	return root

func save_glb(n: Node, path: String) -> void:
	var d := GLTFDocument.new(); var st := GLTFState.new()
	var e := d.append_from_scene(n, st); var e2 := d.write_to_filesystem(st, path)
	print("wrote ", path, " ", e, " ", e2)

func save_tscn(n: Node, path: String, rel: Dictionary) -> void:
	var ps := PackedScene.new(); ps.pack(n)
	ResourceSaver.save(ps, path)
	var t := FileAccess.get_file_as_string(path)
	for k in rel: t = t.replace(k, rel[k])
	var re := RegEx.create_from_string(" uid=\"uid://[a-z0-9]+\""); t = re.sub(t, "", true)
	var f := FileAccess.open(path, FileAccess.WRITE); f.store_string(t); f.close()
	print("wrote ", path)

func _init() -> void:
	var repo: String = OS.get_cmdline_user_args()[0]
	var fx := repo.path_join("tests/fixtures/models")
	var base := base_scene()
	save_tscn(base, repo.path_join("tests/fakegame/game/character/fake_player_char.tscn"), {})
	var b := bean()
	save_glb(b, fx.path_join("bean.glb"))
	var sh: Shader = load(fx.path_join("shaders/tint.gdshader"))
	var sm := ShaderMaterial.new(); sm.shader = sh; sm.set_shader_parameter("albedo", Color(0.1, 0.8, 0.9))
	(b.get_node("Armature/Skeleton3D/beanmesh") as MeshInstance3D).material_override = sm
	save_tscn(b, fx.path_join("bean.tscn"), {fx.path_join("shaders/tint.gdshader"): "shaders/tint.gdshader", "res://tests/fixtures/models/shaders/tint.gdshader": "shaders/tint.gdshader"})
	var bad := NAMES.duplicate(); bad[24] = "lowerarm.R"
	save_glb(bean(bad), fx.path_join("badbones.glb"))
	var sc := bean()
	sc.get_node("Armature/Skeleton3D/beanmesh").set_script(load(fx.path_join("scripts/mark.gd")))
	save_tscn(sc, fx.path_join("scripted.tscn"), {fx.path_join("scripts/mark.gd"): "scripts/mark.gd", "res://tests/fixtures/models/scripts/mark.gd": "scripts/mark.gd"})
	quit()
