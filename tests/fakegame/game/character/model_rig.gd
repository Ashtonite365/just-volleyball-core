class_name ModelRig
extends Node3D




const MODEL := "res://game/character/fake_player_char.tscn"
const TARGET_HEIGHT := 1.8
static var model_hook := Callable()
var hand_radius := 0.0
var hand_rest := Vector3.ZERO
var stencil_id := 1
var _anim: AnimationPlayer
var _skel: Skeleton3D
var _scene: Node3D
var _base_scale := 1.0
var _mats := {}
var _fallback_mat: ShaderMaterial = null
var _face_root: Node3D = null
var _shells := {}
var _surface_slots := {}
var _acc_roots := {}
var _acc_ids := {}
var _bone := {"l": -1, "r": -1}
var _reflection := false
static var _toon: Shader = null

static func available() -> bool: return ResourceLoader.exists(MODEL)

func setup(is_reflection := false) -> bool:
	_reflection = is_reflection
	var scene: Node3D = (load(MODEL) as PackedScene).instantiate()
	if model_hook.is_valid():
		model_hook.call(scene)
	add_child(scene)
	_anim = scene.find_child("AnimationPlayer", true, false)
	var meshes: Array = scene.find_children("*", "MeshInstance3D", true, false)
	var top := 0.01
	for m in meshes: top = maxf(top, (m as MeshInstance3D).mesh.get_aabb().end.y + (m as MeshInstance3D).position.y)
	var k := TARGET_HEIGHT / top
	_scene = scene; _base_scale = k
	scene.scale = Vector3.ONE * k
	for m in meshes:
		var mi := m as MeshInstance3D
		if not String(mi.name).to_lower().contains("hand"): continue
		var box := mi.mesh.get_aabb()
		hand_radius = box.size.z * 0.5 * k
		hand_rest = (mi.position + box.get_center()) * k
		mi.visible = false
	if _toon == null:
		_toon = Shader.new()
		_toon.code = "shader_type spatial;\nrender_mode unshaded;\nuniform vec4 albedo : source_color = vec4(1.0);\nuniform float hidden = 0.0;\nuniform float flash = 0.0;\nuniform vec4 flash_color : source_color = vec4(1.0);\nvoid fragment(){ ALBEDO = mix(albedo.rgb, flash_color.rgb, flash); }"
	for m in meshes:
		var mi := m as MeshInstance3D
		if not mi.visible: continue
		var slot: String = mi.mesh.surface_get_material(0).resource_name
		if not _mats.has(slot):
			var sm := ShaderMaterial.new(); sm.shader = _toon
			sm.set_shader_parameter("albedo", Color(1.0, 0.87, 0.73) if slot == "skin" else Color(0.3, 0.55, 0.95))
			_mats[slot] = sm
		mi.set_surface_override_material(0, _mats[slot])
	_skel = scene.find_child("Skeleton3D", true, false)
	if _reflection: return true
	for m in meshes:
		var mi := m as MeshInstance3D
		if not mi.visible: continue
		var shell := MeshInstance3D.new(); shell.mesh = mi.mesh; shell.name = String(mi.name) + "_shell"; shell.transform = mi.transform
		mi.get_parent().add_child(shell); _shells[mi] = shell
	_bone = {"l": _skel.find_bone("hand.l"), "r": _skel.find_bone("hand.r")}
	_face_root = Node3D.new(); _face_root.name = "Face"; add_child(_face_root)
	var face := Label3D.new(); face.text = ":)"; _face_root.add_child(face)
	return true

func set_slot_color(slot: String, color: Color) -> void:
	if _mats.has(slot): (_mats[slot] as ShaderMaterial).set_shader_parameter("albedo", color)

func set_flash(color: Color, amount: float) -> void:
	for s in _mats:
		_mats[s].set_shader_parameter("flash_color", color); _mats[s].set_shader_parameter("flash", amount)

func skin_color() -> Color:
	return _mats["skin"].get_shader_parameter("albedo") if _mats.has("skin") else Color(1.0, 0.87, 0.73)

func set_head_visible(on: bool) -> void:
	_face_root.visible = on
	for m in find_children("*", "MeshInstance3D", true, false):
		if String(m.name).to_lower().contains("head"): m.visible = on

func play_clip(clip: String) -> void:
	_anim.play(clip)

func hands_animated() -> bool: return _skel != null and _bone["l"] >= 0 and _bone["r"] >= 0



static var hand_local_bad := 0
func hand_local(right: bool) -> Vector3:
	var b: int = _bone["r" if right else "l"]
	if _skel == null or b < 0:
		hand_local_bad += 1
		return Vector3.ZERO
	return _scene.transform * _skel.get_bone_global_pose(b).origin
