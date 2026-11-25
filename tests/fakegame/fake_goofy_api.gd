extends Node





var calls: Array = []
var loader_map := ""
var layers := {}
var _phys: Resource = null
var _stock := -1.0

func api_version() -> String: return "1.0"
func is_enabled() -> bool:
	var g := get_node_or_null("/root/JVModGate")
	return g != null and g.is_active()
func is_physics_allowed() -> bool: return is_enabled()
func get_loader_map() -> String: return loader_map

func set_loader_map(pack_id: String) -> void:
	calls.append(["set_loader_map", pack_id])
	loader_map = pack_id

func apply_pack_params(pack_id: String, params: Dictionary, version: int = 1) -> Dictionary:
	calls.append(["apply_pack_params", pack_id, params.duplicate(true), version])
	loader_map = pack_id
	var applied := []; var ignored := []
	var keep := {}
	for k in params:
		if String(k).begins_with("_"): continue
		if k == "ball.gravity" or k == "gravity_scale": applied.append(k); keep[k] = params[k]
		else: ignored.append(k)
	layers[pack_id] = keep
	_apply()
	return {"ok": true, "applied": applied, "ignored": ignored, "errors": [], "active": is_physics_allowed()}

func clear_pack_params(pack_id: String) -> bool:
	calls.append(["clear_pack_params", pack_id])
	var had := layers.has(pack_id)
	layers.erase(pack_id)
	_apply()
	return had

func _mul() -> float:
	var p: Dictionary = layers.get(loader_map, {})
	if p.has("ball.gravity"):
		var v = p["ball.gravity"]
		return float(v.get("mul", 1.0)) if v is Dictionary else float(v) / max(_stock, 0.001)
	if p.has("gravity_scale"): return float(p["gravity_scale"])
	return 1.0

func _apply() -> void:
	if _phys == null: _phys = load("res://game/ball/ball_physics.tres"); _stock = _phys.gravity
	var m := _mul()
	var om := _override_mul()
	if om >= 0.0: m = om
	_phys.gravity = _stock * (m if is_physics_allowed() else 1.0)

func _process(_d: float) -> void:
	if _phys != null: _apply()




signal params_changed(changed: Array, effective: Dictionary)
signal layers_changed
signal ball_mode_changed(mode: String, force_defaults: bool)
signal physics_allowed_changed(allowed: bool)
var overrides := {}
var ball_mode := "normal"
func get_schema() -> Array:
	return [{"id": "ball.gravity", "section": "physics", "target": "PHYS", "default": 11.0, "min": 0.0, "max": 55.0, "unit": "m/s^2", "apply": "live", "desc": "stub"}]
func get_param(id: String):
	if id != "ball.gravity": return null
	if _phys == null: _phys = load("res://game/ball/ball_physics.tres"); _stock = _phys.gravity
	return _phys.gravity
func get_params() -> Dictionary:
	var g = get_param("ball.gravity")
	return {} if is_equal_approx(g, _stock) else {"ball.gravity": g}
func get_stock(id: String): return _stock if id == "ball.gravity" else null
func get_layers() -> Array:
	var out := []
	for o in overrides: out.append({"owner": o, "priority": overrides[o].priority, "scope": overrides[o].scope, "params": overrides[o].params})
	return out
func get_ball_mode() -> String: return ball_mode
func set_ball_mode(m: String) -> void:
	ball_mode = m
	ball_mode_changed.emit(m, m == "fixed" and loader_map != "")
func set_override(owner: String, params: Dictionary, priority: int = 500, scope: String = "session") -> Dictionary:
	calls.append(["set_override", owner, params.duplicate(true), priority, scope])
	overrides[owner] = {"params": params.duplicate(true), "priority": priority, "scope": scope}
	_apply()
	var applied := []; var ignored := []
	for k in params: (applied if k == "ball.gravity" else ignored).append(k)
	params_changed.emit(applied, get_params())
	layers_changed.emit()
	return {"ok": true, "applied": applied, "ignored": ignored, "errors": [], "active": is_physics_allowed()}
func clear_override(owner: String) -> bool:
	calls.append(["clear_override", owner])
	var had := overrides.has(owner)
	overrides.erase(owner)
	_apply()
	if had: params_changed.emit(["ball.gravity"], get_params()); layers_changed.emit()
	return had
func _override_mul() -> float:
	var best = null; var best_p := -1
	for o in overrides:
		var e: Dictionary = overrides[o]
		if e.params.has("ball.gravity") and int(e.priority) >= best_p: best = e.params["ball.gravity"]; best_p = int(e.priority)
	if best == null: return -1.0
	if layers.get(loader_map, {}).has("ball.gravity") and best_p < 200: return -1.0
	return float(best.get("mul", 1.0)) if best is Dictionary else float(best) / max(_stock, 0.001)
