extends Node



var _defaults := {}
func _key(pk: Key) -> InputEventKey:
	var e := InputEventKey.new(); e.physical_keycode = pk; return e
func _axis(a: int, v: float) -> InputEventJoypadMotion:
	var e := InputEventJoypadMotion.new(); e.axis = a; e.axis_value = v; return e
func _btn(b: int) -> InputEventJoypadButton:
	var e := InputEventJoypadButton.new(); e.button_index = b; return e
func _mouse(b: int) -> InputEventMouseButton:
	var e := InputEventMouseButton.new(); e.button_index = b; return e
func _init() -> void:
	var t := {
		"move_left": [_key(KEY_A), _axis(0, -1.0)], "move_right": [_key(KEY_D), _axis(0, 1.0)],
		"move_forward": [_key(KEY_W), _axis(1, -1.0)], "move_back": [_key(KEY_S), _axis(1, 1.0)],
		"jump": [_key(KEY_SPACE), _btn(0)], "contact": [_mouse(1), _axis(5, 1.0)], "set": [_key(KEY_E), _btn(2)],
		"dive": [_mouse(2), _btn(1)], "serve": [_key(KEY_R), _btn(3)], "strafe": [_key(KEY_SHIFT), _axis(4, 1.0)],
		"sprint": [_key(KEY_CTRL), _btn(9)], "camera_zoom": [_key(KEY_V), _btn(7)], "aim_recenter": [_key(KEY_Q), _btn(10)],
	}
	for a in t:
		if not InputMap.has_action(a): InputMap.add_action(a, 0.2)
		for e in t[a]: InputMap.action_add_event(a, e)
func _ready() -> void:
	for a in ["move_forward", "move_back", "move_left", "move_right", "sprint", "strafe"]:
		_defaults[a] = InputMap.action_get_events(a).duplicate()

func apply(overrides := {}) -> void:
	for a in _defaults:
		InputMap.action_erase_events(a)
		for e in overrides.get(a, _defaults[a]): InputMap.action_add_event(a, e)
