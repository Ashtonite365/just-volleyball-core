extends Control




const SECTIONS := [["face", "face"], ["body", "body"], ["voice", "voice"], ["hair", "hair"],
	["accessories", "accessories"], ["uniforms", "outfits"], ["presets", "presets"], ["studio", "studio"]]
var section_mode := false
var _player: Node = null
var _rail_btns := {}
const ACCESSORY_SUBS := [["head", "head"], ["face", "face"], ["extra", "extra"]]
var _section := "face"
var _subsel := {"accessories": "head"}
var cfg = null
var _outfit_btn: Button = null

var _content: VBoxContainer
var _rail: VBoxContainer
var picks := 0
func _ready() -> void:
	_rail = VBoxContainer.new(); _rail.name = "Rail"; add_child(_rail)
	for e in SECTIONS:
		var b := UiStyle.rail_tile(e[1], UiStyle.icon("star", true))
		b.pressed.connect(_select_section.bind(e[0])); _rail.add_child(b); _rail_btns[e[0]] = b
	var scroll := ScrollContainer.new(); add_child(scroll)
	_content = VBoxContainer.new(); scroll.add_child(_content)
	_select_section("face")
func _rail_size() -> String: return ""
func _select_section(id: String) -> void:
	_section = id; _paint_rail(); _rebuild_content()
func _paint_rail() -> void:
	for id in _rail_btns: UiStyle.paint_rail_tile(_rail_btns[id], id == _section)
func _select_sub(id: String) -> void:
	_subsel[_section] = id
	_rebuild_content()
func _rebuild_content() -> void:
	for c in _content.get_children(): _content.remove_child(c); c.queue_free()
	if _section == "accessories":
		var cat := String(_subsel.get("accessories", "head"))
		var hdr := Label.new(); hdr.text = cat + " items"; hdr.set_meta("header", true); _content.add_child(hdr)
		if cfg == null and Engine.has_singleton("GameConfig") == false:
			pass
		var ch = cfg
		if ch == null:
			var gc = get_node_or_null("/root/GameConfig")
			if gc: ch = gc.character
		for it in AccessoryCatalog.for_category(cat, true):
			var b := Button.new()
			b.text = it.name_or_id()
			b.set_meta("acc_id", it.id)
			var slot := cat
			b.pressed.connect(func():
				if ch: ch.set_acc_id(slot, it.id)
			)
			_content.add_child(b)
		return
	for e in SECTIONS:
		if e[0] == _section:
			var l := Label.new(); l.text = "vanilla " + _section; _content.add_child(l)
func cycle_section(dir: int) -> void:
	var ids: Array = _rail_btns.keys()
	_select_section(ids[posmod(ids.find(_section) + dir, ids.size())])
func _react_to_pick() -> void: picks += 1
func _outfit_name() -> String:
	return "showing: captain uniform"
func _group_header(t: String) -> Control:
	var l := Label.new(); l.text = t; l.set_meta("header", true); return l
func _labeled(t: String, c: Control) -> HBoxContainer:
	var r := HBoxContainer.new(); var l := Label.new(); l.text = t; r.add_child(l); r.add_child(c); return r
func _choice_grid(labels: Array, selected: int, on_pick: Callable) -> HFlowContainer:
	var row := HFlowContainer.new()
	for i in labels.size():
		var b := Button.new(); b.text = str(labels[i]); b.set_meta("selected", i == selected)
		var idx := i
		b.pressed.connect(func(): on_pick.call(idx)); row.add_child(b)
	return row
func _swatch_grid(colors: Array, selected: int, on_pick: Callable) -> HFlowContainer:
	var row := HFlowContainer.new()
	for i in colors.size():
		var b := Button.new(); b.set_meta("color", colors[i]); b.set_meta("selected", i == selected)
		var idx := i
		b.pressed.connect(func(): on_pick.call(idx)); row.add_child(b)
	return row
func _color_field(color: Color, on_change: Callable) -> Control:
	var p := ColorPickerButton.new(); p.color = color
	p.color_changed.connect(func(c): on_change.call(c)); return p
