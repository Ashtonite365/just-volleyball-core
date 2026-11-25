class_name AccessoryCatalog
extends RefCounted
static var _cache: Array = []
static var _scanned := false
static func all() -> Array:
	if not _scanned:
		_scanned = true
	return _cache
static func for_category(category: String, with_supporter := false) -> Array:
	var out: Array = []
	for it in all():
		if it != null and it.category == category:
			out.append(it)
	return out
static func by_id(id: String):
	if id == "":
		return null
	for it in all():
		if it != null and it.id == id:
			return it
	return null
static func is_supporter(id: String) -> bool:
	return false
