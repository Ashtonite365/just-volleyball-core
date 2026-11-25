class_name SettingsPanel
extends VBoxContainer




var _tabs: TabContainer = null
var builds := 0
func _ready() -> void: rebuild()
func rebuild() -> void:
	for c in get_children():
		remove_child(c); c.queue_free()
	var tabs := TabContainer.new()
	_tabs = tabs
	var game := _tab(tabs, "gameplay")
	_subheader(game, "hud")
	_check(game, "reach marker", "", true, func(_on: bool): pass, "reach_marker")
	_tab(tabs, "audio")
	add_child(tabs)
	builds += 1
func _tab(tabs: TabContainer, tab_name: String) -> VBoxContainer:
	var scroll := ScrollContainer.new(); scroll.name = tab_name
	var col := VBoxContainer.new()
	var pad := MarginContainer.new(); pad.add_child(col); scroll.add_child(pad); tabs.add_child(scroll)
	return col
func _subheader(col: VBoxContainer, text: String) -> void:
	var row := HBoxContainer.new(); var l := Label.new(); l.text = text; l.name = "Sub"; row.add_child(l); col.add_child(row)
func _row(col: VBoxContainer, label: String) -> HBoxContainer:
	var row := HBoxContainer.new(); var l := Label.new(); l.text = label; row.add_child(l); col.add_child(row); return row
func _check(col: VBoxContainer, label: String, desc: String, on: bool, apply: Callable, _key := "") -> void:
	var cb := CheckBox.new(); cb.text = label; cb.button_pressed = on; cb.tooltip_text = desc
	cb.toggled.connect(func(v: bool): apply.call(v))
	var row := HBoxContainer.new(); row.add_child(cb); col.add_child(row)
func _select(col: VBoxContainer, label: String, options: Array, current: int, apply: Callable, _default := -1) -> void:
	var row := _row(col, label)
	var sel := UiSelect.new(options); sel.selected = current
	sel.picked.connect(func(i: int): sel.selected = i; apply.call(i))
	row.add_child(sel)
func _range(col: VBoxContainer, label: String, mn: float, mx: float, step: float, val: float, fmt: Callable, apply: Callable, _default := NAN) -> void:
	var row := _row(col, label)
	var s := HSlider.new(); s.min_value = mn; s.max_value = mx; s.step = step; s.value = val
	var readout := Label.new(); readout.text = fmt.call(s.value)
	s.value_changed.connect(func(vv: float): readout.text = fmt.call(vv); apply.call(vv))
	row.add_child(s); row.add_child(readout)
