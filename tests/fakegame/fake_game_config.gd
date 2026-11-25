extends Node
var quick_match := false
var ranked_match := false
var net_roster: Array = []
class Char extends RefCounted:
	var accessories := {}
	func acc_id(slot: String) -> String:
		return String((accessories.get(slot, {}) as Dictionary).get("id", ""))
	func set_acc_id(slot: String, id: String) -> void:
		if id == "":
			accessories.erase(slot)
			return
		accessories[slot] = {"id": id}
	func save_local() -> void:
		pass
	func look_dict() -> Dictionary:
		return accessories.duplicate(true)
var character := Char.new()
var freeplay_stage := ""
