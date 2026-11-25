class_name UiSelect
extends Button
signal picked(index: int)
var options: Array = []
var selected := -1:
	set(v): selected = v; text = str(options[v]) if v >= 0 and v < options.size() else ""
func _init(opts: Array = []) -> void: options = opts
