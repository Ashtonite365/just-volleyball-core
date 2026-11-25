extends RefCounted




var d: Node
const M := "devtest"
const PG := "res://game/player/player.gd"

func api() -> Object:
	return Engine.get_singleton("JVModAPI") if Engine.has_singleton("JVModAPI") else null

func codes(r) -> Array:
	var out := []
	var fs = r.get("findings", []) if r is Dictionary else r
	for f in fs: out.append(f["code"])
	return out

func run(driver: Node) -> void:
	d = driver
	await d.wait(0.9)
	var a := api()
	d.check(a != null and a.get_api_version() == "1.3.0", "API 1.3.0")
	if a == null: return
	var r: Dictionary = a.register_mod(M, "1.3", PackedStringArray(["dev.profiler", "dev.debugger", "player.consts_check", "api.info"]))
	d.check(r["ok"] and (r["denied"] as PackedStringArray).is_empty(), "register with dev.profiler / dev.debugger / player.consts_check")
	var dev: Node = d.get_node_or_null("/root/JVModGate/JVDevTools")
	d.check(dev != null and not dev.is_processing() and not dev.is_physics_processing(), "JVDevTools idle by default (no _process / _physics_process)")
	var tl: Node = d.get_node_or_null("/root/JVTextureLoader")
	d.check(tl != null and tl.has_method("_jv_dev_tap") and not bool(tl._jv_dev_tap("info", null)["on"]), "loader tap present and off")

	var ps0: Dictionary = a.profiler_status(M)
	d.check(not bool(ps0["hotkeys"]) and str(ps0["hotkeys_source"]) == "build" and not bool(ps0["dev_build"]) and str(ps0["profiler_key"]) == "F6" and str(ps0["console_key"]) == "F11", "hotkeys off by default (normal build, auto): %s" % ps0)
	var tg: Dictionary = a.profiler_toggle(M)
	d.check(tg.get("ok", false) and tg.get("toggled", "") == "started" and is_equal_approx(float(tg.get("seconds", 0)), 2.0), "profiler_toggle starts a 2 s capture: %s" % tg)
	await d.wait(0.15)
	var tg2: Dictionary = a.profiler_toggle(M)
	d.check(tg2.get("toggled", "") == "stopped" and not a.profiler_status(M)["capturing"], "profiler_toggle again stops it")
	var st: Dictionary = a.profiler_start(M, 0.6, "fake test")
	d.check(st.get("ok", false), "profiler_start 0.6 s: %s" % st)
	d.check(not a.profiler_start(M, 1.0, "again").get("ok", true), "second start while capturing refused")
	d.check(a.profiler_status(M)["capturing"] and bool(tl._jv_dev_tap("info", null)["on"]), "status capturing, library taps on")
	await d.wait(1.2)
	var stt: Dictionary = a.profiler_status(M)
	d.check(not stt["capturing"] and int(stt["captures"]) == 2, "capture finished by itself: %s" % stt)
	var cap: Dictionary = a.profiler_last_capture(M, true)
	var meta: Dictionary = cap.get("meta", {})
	d.check(int(meta.get("frames", 0)) >= 10 and float(meta.get("seconds", 0)) > 0.4 and float(meta.get("seconds", 0)) < 1.0, "capture: %d frames over %.2f s" % [int(meta.get("frames", 0)), float(meta.get("seconds", 0))])
	var stages: Dictionary = cap.get("stages", {})
	d.check(stages.has("textures.process") and stages.has("core.api_process") and stages.has("core.gate_process"), "per-library stages recorded: %s" % str(stages.keys()).left(300))
	d.check(stages.has("shots.physics") or stages.has("goofy.physics") or stages.has("shots.process"), "shots / goofy hooks recorded")
	var fr: Array = cap.get("frames", [])
	var f0: Dictionary = fr[1] if fr.size() > 1 else {}
	d.check(f0.has("frame_ms") and f0.has("physics_ms") and f0.has("process_ms") and f0.has("render_cpu_ms") and f0.has("game_process_ms") and f0.has("stages"), "frame rows carry built-in stages + mod stages")
	var sums := 0.0
	for row in fr: sums += float(row["frame_ms"])
	d.check(absf(sums / 1000.0 - float(meta["seconds"])) < 0.02, "frames tile the capture window (%.3f s vs %.3f s)" % [sums / 1000.0, float(meta["seconds"])])
	var ok_parts := true
	for row in fr:
		if float(row["process_ms"]) + float(row["physics_ms"]) > float(row["frame_ms"]) + 0.5: ok_parts = false
		if float(row["mods_process_ms"]) > float(row["process_ms"]) + 0.2: ok_parts = false
	d.check(ok_parts, "per-frame parts never exceed the frame; mod hooks fit inside process")
	d.check(FileAccess.file_exists(meta.get("json", "")) and FileAccess.file_exists(meta.get("csv", "")) and String(meta.get("json", "")).begins_with("user://jv_profiler/"), "JSON + CSV written to user://jv_profiler/ (%s)" % meta.get("json", ""))
	var csv := FileAccess.get_file_as_string(meta.get("csv", ""))
	var lines := csv.split("\n", false)
	d.check(lines.size() == int(meta["frames"]) + 1 and lines[0].begins_with("i,t_ms,frame_ms,physics_ms") and "textures.process" in lines[0], "CSV: header + one row per frame")
	var js = JSON.parse_string(FileAccess.get_file_as_string(meta.get("json", "")))
	d.check(js is Dictionary and js.has("summary") and js.has("worst") and js.has("hitches") and js["frames"].size() == int(meta["frames"]), "JSON parses: summary, worst, hitches, frames")
	d.check(a.profiler_summary(M).get("summary", {}).has("frame_ms") and a.profiler_worst_frames(M, 3).size() == 3 and a.profiler_hitches(M).has("threshold_ms"), "summary / worst_frames(3) / hitches via API")
	d.check(a.profiler_last_capture(M).has("frames") == false, "last_capture without frames by default")
	await d.wait(0.1)
	d.check(not dev.is_physics_processing() and not bool(tl._jv_dev_tap("info", null)["on"]) and int(tl._jv_dev_tap("info", null)["buffered"]) == 0, "idle again after the capture (taps off, buffers freed)")

	a.profiler_start(M, 10.0, "stop")
	await d.wait(0.3)
	var sp: Dictionary = a.profiler_stop(M)
	d.check(int(sp.get("meta", {}).get("frames", 0)) > 3 and not a.profiler_status(M)["capturing"], "profiler_stop ends early and keeps the capture")
	d.check(a.profiler_start("nobody", 1.0, "").get("ok", true) == false, "unregistered mod refused")

	d.check(not a.debug_is_enabled(M), "debugger off by default")
	d.check(a.debug_set_enabled(M, true) and a.debug_is_enabled(M), "debug_set_enabled(true)")
	d.check(a.debug_set_log_level(M, "shots", 3) and int(Engine.get_meta("jv_log_levels", {}).get("shots", -1)) == 3 and int(a.debug_log_levels(M)["shots"]) == 3, "per-library log level shared through Engine meta")
	a.debug_set_log_level(M, "shots", -1)
	d.check(not a.debug_log_levels(M).has("shots"), "log level reset (-1)")
	var caps: Dictionary = a.debug_capabilities(M)
	var found := false
	for c in caps.get("capabilities", []):
		if c["id"] == "dev.profiler" and c["status"] == "impl": found = true
	d.check(found and caps.get("libraries", {}).get("textures", {}).get("present", false) and caps.has("by_status"), "debug_capabilities: table + libraries + counts")
	var tree = a.debug_tree(M, "", 1)
	var names := []
	for n in tree: names.append(n["name"])
	d.check("JVModGate" in names and "JVTextureLoader" in names and not ("Lobby" in names), "debug_tree: our nodes only (%s)" % str(names).left(200))
	var nd: Dictionary = a.debug_node(M, "/root/JVModGate/JVDevTools")
	d.check(nd.get("class", "") == "JVDevTools" and nd.has("properties"), "debug_node on our node")
	d.check(a.debug_node(M, "/root/Lobby").is_empty(), "debug_node refuses game nodes")
	a.can("nobody", "dev.profiler"); a.profiler_status("nobody")
	var tr: Array = a.debug_trace(M, 50)
	var kinds := []
	for t in tr: kinds.append(t["kind"])
	d.check("refused" in kinds, "event trace records refusals (%s)" % str(kinds).left(160))
	var dp: String = a.debug_dump(M)
	d.check(dp != "" and FileAccess.file_exists(dp), "debug_dump -> %s" % dp)
	d.check(a.debug_console_toggle(M) and dev.get_child_count() >= 2, "console opens")
	a.debug_console_toggle(M)

	var saved_rally = d._rally
	d._rally = null
	await d.wait(0.6)
	var live: Dictionary = a.consts_live(M)
	d.check(not live.get("available", true) and String(live.get("source", "")).begins_with("direct"), "no player script loaded -> unavailable (%s)" % live.get("reason", ""))
	var w: Dictionary = a.consts_check_write(M, "SET_DUMP_RISE_TAP", 2.0)
	d.check(not w["ok"] and "CAP_UNAVAILABLE" in codes(w), "write while unavailable -> CAP_UNAVAILABLE")
	d.check("PROTOCOL_NAME" in codes(a.consts_check_write(M, "NET_FLOATS", 9)), "NET_FLOATS -> PROTOCOL_NAME")
	d.check("PROTOCOL_NAME" in codes(a.consts_check_write(M, "CALLOUT_SET", 2)), "CALLOUT_* id -> PROTOCOL_NAME")
	d.check("NOT_GAMEPLAY" in codes(a.consts_check_write(M, "DIVE_DENY_TIME", 1.0)), "DIVE_DENY_TIME (visual) -> NOT_GAMEPLAY")
	d.check("UNKNOWN_NAME" in codes(a.consts_check_write(M, "NOT_A_CONST", 1.0)), "unknown -> UNKNOWN_NAME")
	var P: GDScript = load(PG)
	live = a.consts_live(M)
	d.check(live.get("available", false) and live["statics"].size() == 34 and "NET_FLOATS" in live["consts"], "fixture loaded: 34 statics live, protocol names still const")
	w = a.consts_check_write(M, "SET_DUMP_RISE_TAP", 2.0)
	d.check(w["ok"], "valid pre-match write accepted: %s" % str(codes(w)))
	d.check("BAD_TYPE" in codes(a.consts_check_write(M, "JUMP_SPIKE_GRACE_US", 1.5)), "int name with a fraction -> BAD_TYPE")
	d.check("BAD_TYPE" in codes(a.consts_check_write(M, "SPIKE_RECOVER", "fast")), "string -> BAD_TYPE")
	d.check("BAD_VALUE" in codes(a.consts_check_write(M, "SPIKE_RECOVER", INF)), "inf -> BAD_VALUE")
	P.set("SET_DUMP_RISE_TAP", 2.0)
	a.consts_clear_findings(M)

	d._rally = load("res://fake_rally.gd").new()
	await d.wait(0.7)
	var cs: Dictionary = a.consts_status(M)
	d.check(cs["match_active"] and cs["frozen"] and cs["start_set_valid"] and cs["start_set"].size() == 34 and is_equal_approx(float(cs["start_set"]["SET_DUMP_RISE_TAP"]), 2.0), "match start: 34 values captured and frozen")
	w = a.consts_check_write(M, "SPIKE_RECOVER", 1.2)
	d.check(not w["ok"] and "MID_MATCH_WRITE" in codes(w), "any mid-match write -> MID_MATCH_WRITE")
	P.set("BLOCK_TURN", 3.0)
	var au: Dictionary = a.consts_audit(M)
	d.check(not au["ok"] and "MID_MATCH_CHANGE" in codes(au) and "UNTRACKED_WRITE" in codes(au), "live change mid-match -> MID_MATCH_CHANGE + UNTRACKED_WRITE (%s)" % str(codes(au)))
	P.set("BLOCK_TURN", 2.0)
	d.check(a.consts_audit(M)["ok"], "restored -> audit clean")
	var fl: Array = a.consts_findings(M)
	d.check(fl.size() >= 3 and fl.any(func(f): return f["code"] == "MID_MATCH_WRITE" and f["severity"] == "error"), "findings kept with severity / count")
	d._rally = null
	await d.wait(0.7)
	d.check(not a.consts_status(M)["match_active"] and a.consts_check_write(M, "SPIKE_RECOVER", 1.2)["ok"], "match end: writes allowed again")
	P.set("SPIKE_RECOVER", 0.75)

	var stub := Node.new()
	stub.name = "JVPlayerConsts"
	stub.set_script(load("res://devtools_consts_stub.gd"))
	d.get_node("/root/JVModGate").add_child(stub)
	live = a.consts_live(M)
	d.check(String(live.get("source", "")) == "JVPlayerConsts.get_live_statics", "adapter prefers the bypass read API")
	w = a.consts_check_write(M, "SET_DUMP_RISE_TAP", 50.0)
	d.check("OUT_OF_RANGE" in codes(w), "clamp from the read API -> OUT_OF_RANGE")
	au = a.consts_audit(M)
	d.check("FOLDED_COPY_STALE" in codes(au) and "UNTRACKED_WRITE" in codes(au), "BLOCK_TAPE_SLACK changed -> folded copy warning: %s" % str(codes(au)))
	stub.queue_free()
	d._rally = saved_rally
	a.debug_set_enabled(M, false)
	d.check(not a.debug_is_enabled(M) and a.debug_trace(M, 10).is_empty(), "debugger off clears the trace")
