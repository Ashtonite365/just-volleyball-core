extends Node

func get_live_statics() -> Dictionary:
	var st := {}
	for n in ["SET_DUMP_RISE_TAP", "BLOCK_TAPE_SLACK", "SPIKE_RECOVER"]:
		st[n] = {"value": 1.0, "stock": 1.0, "min": 0.0, "max": 4.0, "type": "float"}
	st["BLOCK_TAPE_SLACK"]["value"] = 1.5
	return {"available": true, "reason": "", "statics": st, "consts": PackedStringArray(["NET_FLOATS"])}
