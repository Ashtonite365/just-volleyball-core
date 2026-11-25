extends RefCounted


var d: Node
var changes: Array = []

func api() -> Object:
	return Engine.get_singleton("JVModAPI") if Engine.has_singleton("JVModAPI") else null

func _on_access(access: String, open: bool, _reason: String) -> void:
	changes.append([access, open])

func run(driver: Node) -> void:
	d = driver
	await d.wait(0.9)
	var a := api()
	d.check(a != null, "API registered as Engine singleton JVModAPI")
	d.check(d.get_node_or_null("/root/JVModGate/JVModAPI") == a, "API node at /root/JVModGate/JVModAPI")
	if a == null: return
	d.check(a.get_api_version() == "1.4.0", "API version 1.4.0")
	var caps: Array = a.get_capabilities()
	var cov: Dictionary = a.get_coverage()
	d.check(caps.size() == int(cov["capabilities"]) and caps.size() >= 40, "capability list (%d) = coverage count" % caps.size())
	d.check(int(cov["impl"]) + int(cov["mapped"]) + int(cov["planned"]) == caps.size(), "coverage adds up %s" % cov)
	var ids := {}
	for c in caps: ids[c["id"]] = c
	d.check(ids.has("ball.physics") and ids["ball.physics"]["access"] == "gameplay" and String(ids["ball.physics"]["game_refs"]).find("ball.gd:") != -1, "ball.physics: gameplay, cites ball.gd")
	d.check(ids.has("stage.lobby_picker") and ids["stage.lobby_picker"]["status"] == "planned" and not a.has_capability("stage.lobby_picker"), "planned capability listed but not available")
	d.check(ids.has("net.content_offer") and ids["net.content_offer"]["status"] == "impl" and ids["net.content_offer"]["access"] == "sync" and ids.has("net.content_download") and ids["net.content_download"]["access"] == "sync", "content sharing capabilities implemented, access class sync")

	var mods: Dictionary = a.get_mods()
	d.check(mods.has("goofy") and (mods["goofy"]["declared"] as PackedStringArray).size() == 31, "goofy registered with 31 capabilities")
	d.check(d.status().get("api", {}).get("version", "") == "1.4.0" and "goofy" in d.status()["api"]["mods"], "status.json has the API summary")

	var r: Dictionary = a.register_mod("gdtest", "1.0", PackedStringArray(["scene.balls", "scene.players", "game.generic", "net.lobby_data", "ball.physics", "ball.flight", "nope.cap", "stage.lobby_picker"]))
	d.check(r["ok"] and (r["granted"] as PackedStringArray).size() == 6 and (r["denied"] as PackedStringArray).size() == 2, "GDScript mod registers; unknown + planned refused (%s)" % [r["denied"]])
	d.check(not a.register_mod("future", "2.0", PackedStringArray(["scene.balls"]))["ok"], "mod wanting API 2.0 refused")
	d.check(not a.register_mod("newer", "1.5", PackedStringArray(["scene.balls"]))["ok"], "mod wanting API 1.5 refused (this is 1.4)")
	a.access_changed.connect(_on_access)

	d.set_lobby("offline", -1); await d.wait(0.9)
	d.check(a.get_mode() == "menu" and a.is_open("gameplay") and a.is_open("cosmetic") and a.is_open("offline") and not a.is_open("sync"), "menu: gameplay/cosmetic/offline open, sync closed")
	d.check(a.can("gdtest", "ball.physics") and not a.can("gdtest", "net.lobby_data"), "menu: can ball.physics, not net.lobby_data")
	d.check(a.why_not("gdtest", "net.lobby_data").find("LAN / private / friends-only") != -1, "why_not explains sync rule: " + a.why_not("gdtest", "net.lobby_data"))
	d.check(a.why_not("gdtest", "rules.touch_count").find("did not declare") != -1, "undeclared capability refused")

	var b := Ball.new(); b.name = "ApiBall"; d.add_child(b); await d.frame()
	var balls: Array = a.find_balls("gdtest")
	d.check(b in balls, "find_balls finds the Ball (class_name Ball)")
	d.check(a.find_players("someone_else").is_empty(), "unregistered mod gets nothing")
	d.check(a.ball_set_flight("gdtest", b, {"vel": Vector3(1, 2, 3), "curve": 0.5}) and b.vel == Vector3(1, 2, 3) and is_equal_approx(b.curve, 0.5), "ball_set_flight writes vel/curve")
	d.check(a.ball_get_flight("gdtest", b)["vel"] == Vector3(1, 2, 3), "ball_get_flight reads vel")

	d.check(a.ball_physics_open("gdtest", "") == "", "ball_physics_open (shared PHYS)")
	var r0: float = a.ball_physics_get("gdtest", "radius")
	d.check(a.ball_physics_set("gdtest", "radius", r0 * 2.0) and is_equal_approx(Ball.PHYS.radius, r0 * 2.0), "ball_physics_set edits the game's shared BallPhysics")
	d.check(not a.ball_physics_set("gdtest", "no_such_prop", 1), "unknown BallPhysics property refused")

	var s := GDScript.new()
	s.source_code = "extends Node\nvar x := 1\n@rpc(\"any_peer\")\nfunc boom() -> int: return 7\nfunc ok() -> int: return 8\n"
	s.reload()
	var n: Node = s.new(); d.add_child(n)
	d.check(a.game_get("gdtest", n, "x") == 1 and a.game_set("gdtest", n, "x", 5) and n.x == 5, "game_get/game_set")
	d.check(a.game_call("gdtest", n, "ok", []) == 8, "game_call plain method")
	d.check(a.game_call("gdtest", n, "boom", []) == null, "game_call refuses an @rpc method")
	d.check(a.game_call("gdtest", n, "rpc", ["boom"]) == null, "game_call refuses rpc()")

	changes.clear()
	d.set_lobby("custom", 2); await d.wait(0.3)
	d.check(not a.is_open("gameplay") and a.is_open("cosmetic") and not a.is_open("sync"), "public: gameplay closed, cosmetic open (local only)")
	d.check(changes.has(["gameplay", false]), "access_changed(gameplay, false) emitted")
	d.check(not a.ball_physics_set("gdtest", "radius", r0 * 3.0) and is_equal_approx(Ball.PHYS.radius, r0 * 2.0), "public: ball_physics_set refused")
	d.check(a.ball_physics_restore("gdtest", "radius", r0) and is_equal_approx(Ball.PHYS.radius, r0), "public: restoring own change still allowed")
	d.check(a.game_get("gdtest", n, "x") == null, "public: generic access refused")
	d.check(a.find_balls("gdtest").size() >= 1, "public: read-only queries still work")
	d.check(a.get_mods()["gdtest"]["refusals"].get("game.generic", 0) >= 1, "refusals counted per capability")

	for c in [["custom", 0, "private"], ["custom", 1, "friends-only"], ["lan", -1, "LAN"]]:
		d.set_lobby("offline", -1); await d.wait(0.2)
		d.set_lobby(c[0], c[1]); await d.wait(0.9)
		d.check(a.get_mode() == c[2] and a.is_open("gameplay") and a.is_open("sync") and not a.is_open("offline"), "%s: gameplay + sync open, offline-only closed" % c[2])

	d.set_lobby("offline", -1); await d.wait(0.2)
	d.set_lobby("freeplay", -1); await d.wait(0.9)
	d.check(a.is_open("gameplay") and a.is_open("offline") and a.is_open("cosmetic") and not a.is_open("sync"), "Free Play (%s): gameplay/offline/cosmetic open, sync closed" % a.get_mode())

	d.set_lobby("weird", -1); await d.wait(0.3)
	d.check(not a.is_open("gameplay") and not a.is_open("cosmetic") and a.is_open("read"), "unknown context: only read stays open")

	d.check(not a.is_strict() and a.library_allowed("shots") and a.library_allowed("goofy"), "strict mode off by default")
	d.check(d.gate().api_library_allowed("shots") and d.gate().api_library_allowed("textures"), "bootstrap check: every library loads when not strict")
	a.ball_physics_close("gdtest")
	n.queue_free(); b.queue_free()
	d.set_lobby("offline", -1); await d.wait(0.9)



func run_strict(driver: Node) -> void:
	d = driver
	await d.wait(0.9)
	d.check(d.get_node_or_null("/root/JVGoofyMods") != null, "strict: goofy (migrated) loaded")
	for n in ["JVTextureLoader", "JVExtraShots", "JVModelLoader", "JVLoader"]:
		d.check(d.get_node_or_null("/root/" + n) != null, "strict: %s (migrated to CORE) loaded" % n)
	var libs: Dictionary = d.gate().build_status().get("libraries", {})
	for lib in ["shots", "textures", "models"]:
		var st: String = libs.get(lib, {}).get("state", "")
		d.check(st != "" and st != "disabled-strict-api" and st != "not-loaded", "strict: status says %s runs (%s)" % [lib, st])
	d.check(not d.gate().api_library_allowed("legacy_thing"), "strict: a library not migrated to CORE is refused")
	d.check(api() != null and api().is_strict(), "strict: API reports strict mode")
	var mods: Dictionary = api().get_mods()
	for lib in ["shots", "textures", "models", "loader"]:
		d.check(mods.has(lib), "strict: %s is registered with CORE (%s)" % [lib, mods.keys()])
	d.check(is_equal_approx(Engine.time_scale, 0.5), "strict: goofy still applies through the API (time_scale %.2f)" % Engine.time_scale)
