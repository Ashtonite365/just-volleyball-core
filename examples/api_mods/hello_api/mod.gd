extends Node















const MOD := "hello_api"
const CAPS := ["api.info", "gate.mode", "events.bus", "scene.current", "scene.balls", "ball.struck",
	"stage.pick", "packs.query", "config.state", "goofy.query", "goofy.ball", "menu.perf"]

var api: Object = null
var hits := 0
var hooked := {}
var layer_on := false
var log_lines: Array = []

func say(msg: String) -> void:
	log_lines.append(msg)
	print("[hello_api] ", msg)

func _ready() -> void:
	api = Engine.get_singleton("JVModAPI") if Engine.has_singleton("JVModAPI") else null
	if api == null:
		say("no JVModAPI - nothing to do"); return
	var r: Dictionary = api.register_mod(MOD, "1.1", PackedStringArray(CAPS))
	if not r.ok:
		say("registration refused: " + String(r.reason)); return
	say("registered with API %s, mode %s" % [api.get_api_version(), api.get_mode()])
	hits = int(api.state_get(MOD, "hits_total", 0))
	for ev in ["mode_changed", "scene_changed", "stage_picked", "access_changed", "goofy_params_changed"]:
		api.subscribe(MOD, ev, _on_event)
	api.access_changed.connect(_on_access)
	say("goofy schema: %d params, Goofy API %s" % [api.goofy_schema(MOD).size(), api.goofy_api_version(MOD) if api.goofy_available(MOD) else "not installed"])
	say("stage packs: %d, picked stage '%s'" % [api.packs_list(MOD, "stage").size(), api.stage_get_picked(MOD)])
	_sync_layer()

func _on_event(name: String, data: Dictionary) -> void:
	if name == "access_changed": return
	say("event %s %s" % [name, str(data)])

func _on_access(access: String, open: bool, reason: String) -> void:
	if access == "offline": _sync_layer()

func _sync_layer() -> void:
	var want: bool = api.is_open("offline") and api.goofy_available(MOD)
	if want and not layer_on:
		var res: Dictionary = api.goofy_set_override(MOD, {"ball.gravity": {"mul": 0.85}}, 150, "session")
		layer_on = bool(res.get("ok", false))
		say("low-gravity layer %s" % ("on" if layer_on else "refused: " + str(res.get("errors", []))))
	elif not want and layer_on:
		api.goofy_clear_override(MOD)
		layer_on = false
		say("low-gravity layer cleared (%s)" % api.get_reason("offline"))

func _process(_d: float) -> void:
	if api == null: return
	for b in api.find_balls(MOD):
		var id: int = b.get_instance_id()
		if hooked.has(id): continue
		hooked[id] = true
		api.connect_ball_event(MOD, b, "struck", _on_struck)

func _on_struck(_pos: Vector3, power: float, kind: int) -> void:
	hits += 1
	if hits % 10 == 0: api.state_set(MOD, "hits_total", hits)

func _exit_tree() -> void:
	if api == null: return
	api.state_set(MOD, "hits_total", hits)
	if layer_on: api.goofy_clear_override(MOD)
