class_name ModifiersPanel
extends Control


signal changed(key: String, value: Variant)
var editable := true
var _rows := {}
var _values := {}
var _quiet := false
var list := VBoxContainer.new()
func _ready() -> void:
	add_child(list)
	list.add_child(_entry_row({"id": "serve", "name": "serve assist", "kind": "toggle"}))
	list.add_child(_rule())
	list.add_child(_entry_row({"id": "rate", "name": "game speed", "kind": "choice", "options": ["slow", "normal", "fast"]}))
func _rule() -> Control:
	var d := Panel.new(); d.custom_minimum_size = Vector2(0, 2); return d
func _entry_row(e: Dictionary) -> Control:
	var id := String(e.id)
	var row := HBoxContainer.new()
	var col := VBoxContainer.new(); row.add_child(col)
	var n := Label.new(); n.text = String(e.name); col.add_child(n)
	var d := Label.new(); d.text = ""; col.add_child(d)
	var ctl: Control
	if String(e.kind) == "choice":
		var sel := UiSelect.new((e.options as Array).duplicate())
		sel.picked.connect(func(i: int): _emit(id, i)); ctl = sel
	else:
		var cb := CheckBox.new(); cb.toggled.connect(func(on: bool): _emit(id, on)); ctl = cb
	row.add_child(ctl)
	_rows[id] = ctl
	return row
func _emit(key: String, value: Variant) -> void:
	if _quiet or not editable: return
	_values[key] = value
	changed.emit(key, value)
