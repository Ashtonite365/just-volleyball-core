extends Node



const M := "jv_probe"
func p(m: String) -> void: print("PROBE ", m)

func wait_menu() -> Node:
	var m: Node = null
	for i in 120:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"): m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if i % 10 == 9: p("waiting menu: scene=%s" % m.scene_file_path)
		if m.has_method("_go_to_match") and (m.get("player") != null or i > 40):
			await get_tree().create_timer(3.0).timeout
			return m
	return null

func fmt_top(arr: Array) -> String:
	var s := []
	for e in arr: s.append("%s=%.2f" % [e["stage"], float(e["ms"])])
	return ", ".join(s)

func report(a: Object, tag: String) -> void:
	var cap: Dictionary = a.profiler_last_capture(M, false)
	var meta: Dictionary = cap.get("meta", {})
	p("cap[%s] frames=%d seconds=%.3f avg=%.2fms median=%.2fms worst=%.1fms fps_avg=%.1f renderer=%s scene=%s json=%s" % [tag, int(meta.get("frames", 0)), float(meta.get("seconds", 0)), float(meta.get("avg_frame_ms", 0)), float(meta.get("median_frame_ms", 0)), float(meta.get("worst_frame_ms", 0)), float(meta.get("fps_avg", 0)), meta.get("renderer", ""), meta.get("scene", ""), meta.get("json_abs", "")])
	p("cap[%s] taps=%s outside=%s" % [tag, str(meta.get("taps", [])), str(meta.get("samples_outside_frames", 0))])
	var sm: Dictionary = cap.get("summary", {})
	for k in ["frame_ms", "physics_ms", "process_ms", "game_process_ms", "mods_process_ms", "render_cpu_ms", "other_ms", "game_physics_ms", "mods_physics_ms", "mods_other_ms", "engine_process_ms", "viewport_render_cpu_ms", "viewport_render_gpu_ms"]:
		var s: Dictionary = sm.get(k, {})
		p("cap[%s] builtin %s mean=%.3f p50=%.3f p95=%.3f max=%.3f" % [tag, k, float(s.get("mean_ms", 0)), float(s.get("p50_ms", 0)), float(s.get("p95_ms", 0)), float(s.get("max_ms", 0))])
	var st: Dictionary = cap.get("stages", {})
	var keys := st.keys()
	keys.sort_custom(func(x, y): return float(st[x]["total_ms"]) > float(st[y]["total_ms"]))
	for k in keys.slice(0, 24):
		var s: Dictionary = st[k]
		p("cap[%s] stage %s calls=%d total=%.2f mean/frame=%.3f max=%.3f nonzero_frames=%d" % [tag, k, int(s.get("calls", 0)), float(s["total_ms"]), float(s["mean_ms"]), float(s["max_ms"]), int(s["frames_nonzero"])])
	for w in a.profiler_worst_frames(M, 5):
		p("cap[%s] worst i=%d t=%.0fms frame=%.1fms phys=%.1f(%d ticks) proc=%.1f render=%.1f other=%.1f top: %s" % [tag, int(w["i"]), float(w["t_ms"]), float(w["frame_ms"]), float(w["physics_ms"]), int(w["physics_ticks"]), float(w["process_ms"]), float(w["render_cpu_ms"]), float(w["other_ms"]), fmt_top(w.get("top", []))])
	var h: Dictionary = a.profiler_hitches(M)
	var hl: Array = h.get("list", [])
	p("cap[%s] hitches n=%d threshold=%.1fms interval_mean=%s interval_median=%s intervals=%s" % [tag, hl.size(), float(h.get("threshold_ms", 0)), str(h.get("interval_mean_ms", "-")), str(h.get("interval_median_ms", "-")), str(h.get("intervals_ms", []))])
	for e in hl.slice(0, 12):
		p("cap[%s] hitch t=%.0fms frame=%.1fms top: %s" % [tag, float(e["t_ms"]), float(e["frame_ms"]), fmt_top(e.get("top", []))])
	for e in h.get("explained_by", []):
		p("cap[%s] explained_by %s +%.2fms per hitch frame" % [tag, e["stage"], float(e["extra_ms"])])

func _ready() -> void:
	var stage := OS.get_environment("JV_DT_STAGE")
	if stage == "": stage = "1023"
	var caps_s := OS.get_environment("JV_DT_CAPS")
	if caps_s == "": caps_s = "2,2,4"
	var settle := float(OS.get_environment("JV_DT_SETTLE")) if OS.get_environment("JV_DT_SETTLE") != "" else 12.0
	var m := await wait_menu()
	p("menu=%s" % [m != null])
	if m == null: get_tree().quit(3); return
	var a: Object = Engine.get_singleton("JVModAPI") if Engine.has_singleton("JVModAPI") else null
	p("api=%s version=%s" % [a != null, a.get_api_version() if a else "-"])
	if a == null: get_tree().quit(4); return
	var r: Dictionary = a.register_mod(M, "1.3", PackedStringArray(["dev.profiler", "dev.debugger", "player.consts_check", "packs.pick", "court.env", "textures.query", "ui.mods_panel"]))
	p("register ok=%s denied=%s" % [r.get("ok"), str(r.get("denied"))])

	var dvt: Node = get_node_or_null("/root/JVModGate/JVDevTools")
	var ps0: Dictionary = a.profiler_status(M)
	p("hotkeys default: hotkeys=%s source=%s dev_build=%s key=%s console=%s" % [ps0.get("hotkeys"), ps0.get("hotkeys_source"), ps0.get("dev_build"), ps0.get("profiler_key"), ps0.get("console_key")])
	a.mods_panel_open(M)
	await get_tree().create_timer(1.0).timeout
	var btn: Button = get_tree().root.find_child("JvmDevProfiler", true, false)
	p("menu entry: %s" % (btn.text if btn else "MISSING"))
	if btn:
		btn.pressed.emit(); await get_tree().create_timer(0.1).timeout
		var c1: bool = a.profiler_status(M)["capturing"]
		await get_tree().create_timer(2.8).timeout
		var st1: Dictionary = a.profiler_status(M)
		p("menu entry capture: started=%s finished_by_itself=%s captures=%s last=%s label=%s" % [c1, not st1["capturing"], st1["captures"], String(st1["last_json"]).get_file(), btn.text])
	a.mods_panel_close(M)
	await get_tree().create_timer(1.0).timeout
	var tl: Node = get_node_or_null("/root/JVTextureLoader")
	if stage != "-" and tl:
		var ok = tl.set_picked_stage(stage)
		if tl.has_method("apply_picked_cosmetics"): tl.apply_picked_cosmetics()
		p("pick stage %s ok=%s picked=%s" % [stage, ok, tl.get_picked_stage() if tl.has_method("get_picked_stage") else "?"])
	await get_tree().create_timer(3.0).timeout
	var gc = get_node("/root/GameConfig"); gc.set("cpu_fill", 0)
	if gc.get("match_intro") != null: gc.set("match_intro", false)
	m.call("_go_to_match", 2)
	var main: Node = null
	for i in 60:
		await get_tree().create_timer(1.0).timeout
		var s := get_tree().current_scene
		if s and s.scene_file_path == "res://game/main/main.tscn" and s.get("player") != null: main = s; break
	if main == null: p("no match"); get_tree().quit(5); return
	p("match scene up; settling %.0f s" % settle)
	await get_tree().create_timer(settle).timeout
	if tl and tl.has_method("get_status"):
		var s: Dictionary = tl.get_status()
		p("textures status picked=%s env=%s" % [tl.get_picked_stage() if tl.has_method("get_picked_stage") else "?", str(s.get("court_env", s.get("env", "?"))).left(300)])
	if tl and tl.has_method("get_mip_stats"): p("mip_stats s3tc_state=%s s3tc_warnings=%s" % [tl.get_mip_stats().get("s3tc_state"), tl.get_mip_stats().get("s3tc_warnings")])
	p("court_env_status=%s" % str(a.court_env_status(M)).left(400))
	p("fps_monitor=%.1f nodes=%d" % [Performance.get_monitor(Performance.TIME_FPS), int(Performance.get_monitor(Performance.OBJECT_NODE_COUNT))])
	var n := 0
	for c in caps_s.split(","):
		var secs := float(c)
		var st: Dictionary = a.profiler_start(M, secs, "snow-cpu-%d" % n)
		p("profiler_start %.1f s -> %s" % [secs, str(st)])
		await get_tree().create_timer(secs + 0.6).timeout
		while a.profiler_status(M).get("capturing", false): await get_tree().create_timer(0.2).timeout
		report(a, "%d-%.0fs" % [n, secs])
		n += 1
		await get_tree().create_timer(1.0).timeout

	if dvt:
		var ev := InputEventKey.new(); ev.keycode = KEY_F6; ev.pressed = true
		var cb := int(a.profiler_status(M)["captures"])
		Input.parse_input_event(ev); await get_tree().create_timer(1.5).timeout
		var off_ok: bool = not a.profiler_status(M)["capturing"] and int(a.profiler_status(M)["captures"]) == cb
		dvt.set_hotkeys(true)
		Input.parse_input_event(ev); await get_tree().create_timer(0.2).timeout
		var on1: bool = a.profiler_status(M)["capturing"]
		Input.parse_input_event(ev); await get_tree().create_timer(1.5).timeout
		var st2: Dictionary = a.profiler_status(M)
		p("hotkey F6: inert_while_off=%s start=%s second_press_stops=%s captures=%d->%d" % [off_ok, on1, not st2["capturing"], cb, int(st2["captures"])])
		Input.parse_input_event(ev); await get_tree().create_timer(0.3).timeout
		while a.profiler_status(M).get("capturing", false): await get_tree().create_timer(0.2).timeout
		report(a, "hotkey-2s")
		dvt.set_hotkeys(false)

	var dev: Node = get_node_or_null("/root/JVModGate/JVDevTools")
	p("idle after captures: processing=%s physics=%s" % [dev.is_processing() if dev else "?", dev.is_physics_processing() if dev else "?"])

	a.debug_set_enabled(M, true)
	var caps: Dictionary = a.debug_capabilities(M)
	p("debug caps by_status=%s mode=%s classes=%s" % [str(caps.get("by_status")), caps.get("mode"), str(caps.get("classes"))])
	var tree = a.debug_tree(M, "", 0)
	var names := []
	for t in tree: names.append(t["name"])
	p("debug_tree ours=%s" % str(names).left(400))
	var cs: Dictionary = a.consts_status(M)
	p("consts_status available=%s reason=%s source=%s bypass_active=%s match_active=%s frozen=%s still_const=%s" % [cs.get("available"), cs.get("reason"), cs.get("source"), cs.get("bypass_active"), cs.get("match_active"), cs.get("frozen"), cs.get("still_const")])
	var w: Dictionary = a.consts_check_write(M, "SET_DUMP_RISE_TAP", 3.0)
	var codes := []
	for f in w.get("findings", []): codes.append(f["code"])
	p("consts_check_write mid-match ok=%s codes=%s" % [w.get("ok"), str(codes)])
	p("debug_dump=%s trace_n=%d" % [a.debug_dump(M), a.debug_trace(M, 512).size()])
	p("DONE")
	get_tree().quit()
