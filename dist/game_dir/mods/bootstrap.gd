extends Node




const BIN := "res://mods/bin/"
const CORE := "jvm_core"
const CATEGORIES := {
	"jvm_loader": "JVLoader",
	"jvm_shots": "JVExtraShots",
	"jvm_goofy": "JVGoofyMods",
}

const SUBSYSTEMS := {"jvm_loader": ["textures", "models"]}


const LEGACY := {"jvm_textures": "JVTextureLoader", "jvm_models": "JVModelLoader"}

func _log(msg: String) -> void:
	print("[jv-mods:bootstrap] ", msg)

func _load_ext(name: String) -> bool:
	var path := BIN + name + ".gdextension"
	if not FileAccess.file_exists(path):
		_log("missing " + path)
		return false
	var st := GDExtensionManager.load_extension(path)
	if st != GDExtensionManager.LOAD_STATUS_OK and st != GDExtensionManager.LOAD_STATUS_ALREADY_LOADED:
		_log("failed to load %s (status %d)" % [path, st])
		return false
	return true






func _init() -> void:
	if Engine.has_meta("jv_mods_pc_boot"):
		return
	Engine.set_meta("jv_mods_pc_boot", true)
	if not FileAccess.file_exists(BIN + "jvm_goofy.gdextension"):
		return
	if not _load_ext(CORE) or not ClassDB.class_exists("JVPlayerConsts"):
		return
	var pc: RefCounted = ClassDB.instantiate("JVPlayerConsts")
	var r: Dictionary = pc.prepare_at_boot()
	_log("player.consts: " + ("mounted" if r.get("mounted", false) else "stock (" + str(r.get("reason", "?")) + ")"))

func _ready() -> void:
	if Engine.has_meta("jv_mods_booted"):
		queue_free()
		return
	Engine.set_meta("jv_mods_booted", true)
	_log("Godot " + Engine.get_version_info().string)
	if not _load_ext(CORE) or not ClassDB.class_exists("JVModGate"):
		_log("core not loaded -> no mods (fail closed)")
		return
	var root := get_tree().root
	var gate: Node = ClassDB.instantiate("JVModGate")
	gate.name = "JVModGate"


	var pinned: bool = gate.verify_version()
	root.add_child.call_deferred(gate)
	var libs := CATEGORIES.duplicate()
	if not FileAccess.file_exists(BIN + "jvm_loader.gdextension"):
		libs.erase("jvm_loader")
		libs.merge(LEGACY)
		_log("jvm_loader missing -> using the old jvm_textures + jvm_models")
	else:
		for old in LEGACY:
			if FileAccess.file_exists(BIN + old + ".gdextension"):
				_log("obsolete %s.gdextension present (not loaded; replaced by jvm_loader) - safe to remove" % old)
	for lib in libs:
		var cls: String = libs[lib]
		var short: String = lib.trim_prefix("jvm_")
		if not pinned:
			gate.register_library(short, "disabled-version-mismatch", "game version not pinned; library not loaded")
			for sub in SUBSYSTEMS.get(lib, []):
				gate.register_library(sub, "disabled-version-mismatch", "game version not pinned; jvm_loader not loaded")
			continue


		if gate.has_method("api_library_allowed") and not gate.api_library_allowed(short):
			gate.register_library(short, "disabled-strict-api", "strict API mode: library not migrated to the mod API yet")
			for sub in SUBSYSTEMS.get(lib, []):
				gate.register_library(sub, "disabled-strict-api", "strict API mode: jvm_loader not migrated to the mod API yet")
			continue
		if _load_ext(lib) and ClassDB.class_exists(cls):
			var n: Node = ClassDB.instantiate(cls)
			n.name = cls
			root.add_child.call_deferred(n)
			gate.register_library(short, "loaded", "")
			_log("loaded " + cls)
		else:
			gate.register_library(short, "not-loaded", "extension failed to load")
			for sub in SUBSYSTEMS.get(lib, []):
				gate.register_library(sub, "not-loaded", "jvm_loader failed to load")
	if not pinned:
		_log("game version mismatch -> mod libraries NOT loaded (game runs unmodded)")
