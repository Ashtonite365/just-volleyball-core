class_name Player
extends CharacterBody3D

signal touched(kind: int)
signal swung(kind: int)
signal jumped(bam: bool)
signal launched(pos: Vector3, charge: float)

var stamina := 100.0
var sprinting := false
var dive_cooldown := 0.0
var _model: FakeRig = FakeRig.new()
var _reflect_root: Node3D = null
var character: FakeChar = FakeChar.new()
class FakeRig extends RefCounted:
	func anim_state() -> Dictionary: return {"clip": "idle", "pos": 0.25, "speed": 1.0}
class FakeChar extends RefCounted:
	var display_name := "Fakey"
	var number := 7
	var accessories := {"head": {"id": "cap_red"}}
	func skin_tone() -> Color: return Color(0.8, 0.6, 0.5)
	func hair_style_name() -> String: return "buzz"
	func hair_color() -> Color: return Color(0.1, 0.1, 0.1)
	func acc_id(slot: String) -> String: return str(accessories.get(slot, {}).get("id", ""))
	func look_dict() -> Dictionary: return {"face": 0, "skin": 3}
var ball: Ball = null
var is_bot := false
var team_side := 1
var touch_barred := false
var touch_bar_reason := 0
var serving := false
var shared_ball_off := false
var aim_point := Vector3(0, 0, -6)
var sim_authority := true
var net_puppet := false
var airborne := false
func grounded() -> bool: return not airborne


func do_touch(kind: int, power := 0.7) -> bool:
	if touch_bar_reason == Rally.BAR_CONSECUTIVE: return false
	ball.strike_source = self
	ball.hit(aim_point, kind, power, false)
	if sim_authority: touched.emit(kind)
	return true


var fake_hold := 0.5
var last_perfect := false
var fake_quality := 0.8
func hold_power() -> float: return fake_hold
func _receive_quality() -> float: return fake_quality
