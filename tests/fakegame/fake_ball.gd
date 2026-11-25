class_name Ball
extends Node3D


static var PHYS = preload("res://game/ball/ball_physics.tres")
signal struck(pos: Vector3, power: float, kind: int)
signal bounced
signal antenna_hit(side: int)
var hit_by: Player = null
var strike_source: Player = null
var vel := Vector3.ZERO
var curve := 0.0
var float_serve := false
var float_seed := 0
var tip_drop := false
var dive_g := 1.0
var net_puppet := false
var remote_driven := false
var frozen := true
var strike_seq := 0
var serve_held := false
var hitstop_left := 0.0
var net_half_width: float = PHYS.net_half_width


var _land_predict: MeshInstance3D = null
var _land_shell: MeshInstance3D = null
var _predict_spread := -1.0
var tint_fade := 1.0
const SPREAD_FAR := 1.15
const SPREAD_NEAR := 0.42
const FLOAT_MARK_SPREAD := 1.9
const FLOAT_MARK_FROM := 0.12
const FLOAT_MARK_GONE := 0.35
const FLOAT_MARK_FROM_STILL := 0.05
const FLOAT_MARK_GONE_STILL := 0.25


var mesh: Node3D = null
var _model: Node3D = null
func _ready() -> void:
	mesh = Node3D.new(); mesh.name = "Mesh"; add_child(mesh)
	_model = Node3D.new(); mesh.add_child(_model)
	var body := Node3D.new(); _model.add_child(body)
	var mi := MeshInstance3D.new(); var sm := SphereMesh.new(); sm.radius = 0.5; sm.height = 1.0; mi.mesh = sm; body.add_child(mi)
	body.scale = Vector3.ONE * (PHYS.radius * 2.0 / 1.0)
	_land_predict = MeshInstance3D.new(); _land_predict.top_level = true; _land_predict.visible = false; add_child(_land_predict)
	_land_shell = MeshInstance3D.new(); _land_predict.add_child(_land_shell)
func _shard_cluster_mesh(_inset := 0.0, spread := SPREAD_FAR) -> Mesh:
	var m := BoxMesh.new(); m.size = Vector3(spread * 1.4142, 0.01, spread * 1.4142); return m
func _land_tint(_col: Color, fade := 1.0) -> void: tint_fade = fade
func _time_to_floor() -> float:
	if vel.y >= 0.0: return -1.0
	return float_fall_time(global_position.y, vel.y, PHYS.radius, 1.0)
static func float_fall_time(y: float, vy: float, floor_y: float, _charge: float) -> float:
	var g: float = PHYS.gravity
	return (vy + sqrt(maxf(vy * vy + 2.0 * g * (y - floor_y), 0.0))) / g
static func float_offset(seed: int, t: float, _v: Vector3) -> Vector3:
	return Vector3(sin(t * 7.0 + seed) * 0.3, 0.0, cos(t * 5.0 + seed) * 0.3)
static func float_charge_of(seed: int) -> float: return float((seed >> 4) & 15) / 8.0
static func solve_float(from: Vector3, target: Vector3, _power: float, _charge := 1.0) -> Vector3:
	var t := 1.5; var d := target - from; var g: float = PHYS.gravity
	return Vector3(d.x / t, (d.y + 0.5 * g * t * t) / t, d.z / t)
func box_spread() -> float: return _predict_spread
func visual_radius() -> float:
	return float(PHYS.radius if _model == null else _model.get_child(0).scale.x * 0.5) * global_basis.get_scale().x

var court_center_x := 0.0
var court_half_w := 4.5
var court_half_l := 9.0
const CURVE_REF := 12.0
const CURVE_DECAY := 1.2
static func solve_launch(from: Vector3, target: Vector3, _kind: int, _power: float, _finesse: bool, _influence := Vector2.ZERO, _c := false, _q := false, _s := false, _t := false) -> Vector3:
	var t := 1.2
	var d := target - from
	var g: float = PHYS.gravity
	return Vector3(d.x / t, (d.y + 0.5 * g * t * t) / t, d.z / t)
func _predict_land_curved() -> Vector3:
	var p := global_position; var v := vel; var c := curve
	for i in 400:
		var flat := Vector3(v.x, 0, v.z); var sp := flat.length()
		if sp > 1.0: v += Vector3.UP.cross(flat / sp) * c * (sp / CURVE_REF) / 60.0
		c *= exp(-1.0 / 60.0 / CURVE_DECAY)
		v.y -= PHYS.gravity / 60.0
		var before := p
		p += v / 60.0
		if before.y > 0.0 and p.y <= 0.0: break
	return Vector3(p.x, 0, p.z)

func hit(target: Vector3, kind: int, power: float, _finesse: bool, _influence := Vector2.ZERO, _ca := false, _quick := false, float_seed_in := 0, _tower := false) -> void:
	var d := target - global_position
	d.y = 0.0
	vel = d.normalized() * 8.0 * power + Vector3.UP * 6.0
	hit_by = strike_source
	strike_seq += 1
	stop_float()
	if float_seed_in != 0: start_float(float_seed_in)
	struck.emit(global_position, power, kind)

func start_float(seed: int) -> void:
	float_serve = true; float_seed = seed; curve = 0.0
func stop_float() -> void:
	float_serve = false; tip_drop = false; dive_g = 1.0
static func float_seed_for(pattern: int, charge: float) -> int:
	return (roundi(clampf(charge, 0.0, 1.0) * 8.0) << 4) | maxi(pattern & 15, 1)

func _physics_process(dt: float) -> void:
	if not frozen:
		vel.y -= PHYS.gravity * dive_g * dt
		position += vel * dt
	var airborne: bool = not frozen and global_position.y > float(PHYS.radius) + 0.3
	_land_predict.visible = airborne
	if not airborne: return
	var land := _predict_land_curved()
	_land_predict.global_position = Vector3(land.x, 0.045, land.z)
	_land_tint(Color.WHITE, 0.5 if float_serve else 1.0)
	var spread: float = maxf(SPREAD_FAR * 2.0, SPREAD_FAR * FLOAT_MARK_SPREAD) if float_serve else SPREAD_FAR
	if absf(spread - _predict_spread) > 0.01:
		_predict_spread = spread; _land_predict.mesh = _shard_cluster_mesh(0.22, spread)
