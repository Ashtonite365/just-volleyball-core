extends Node


func p(msg: String) -> void: print("PROBE ", msg)

const MON := {
	"proc": 1, "phys": 2, "objects": 11, "prims": 12, "draws": 13, "nodes": 9,
}

func summarize(tag: String, frames: Array, mons: Dictionary) -> void:
	if frames.is_empty():
		p("FRAMES %s n=0" % tag)
		return
	var copy: Array = frames.duplicate()
	copy.sort()
	var sum := 0.0
	var mx: float = copy[0]
	var over16 := 0
	var over8 := 0
	for v in copy:
		sum += float(v)
		if float(v) > mx: mx = float(v)
		if float(v) >= 8.0: over8 += 1
		if float(v) >= 16.67: over16 += 1
	var n := copy.size()
	var mean := sum / n
	var p50: float = copy[mini(n - 1, int(round(0.50 * (n - 1))))]
	var p95: float = copy[mini(n - 1, int(round(0.95 * (n - 1))))]
	var p99: float = copy[mini(n - 1, int(round(0.99 * (n - 1))))]
	p("FRAMES %s n=%d mean=%.3fms (%.1ffps) p50=%.3f p95=%.3f p99=%.3f max=%.3f >8=%d >16.7=%d" % [tag, n, mean, (1000.0 / mean) if mean > 0.0 else 0.0, p50, p95, p99, mx, over8, over16])
	var bits: PackedStringArray = []
	for k in ["proc", "phys", "draws", "objects", "prims", "nodes"]:
		var arr: Array = mons.get(k, [])
		if arr.is_empty():
			continue
		var s := 0.0
		for v in arr: s += float(v)
		var unit := "ms" if k in ["proc", "phys"] else ""
		var scale := 1000.0 if k in ["proc", "phys"] else 1.0
		bits.append("%s=%.2f%s" % [k, (s / arr.size()) * scale, unit])
	p("MONITORS %s %s tweens_end=%d" % [tag, " ".join(bits), mons.get("tweens", -1)])

func collect(secs: float, each: Callable) -> Dictionary:
	var frames: Array[float] = []
	var mons := {"proc": [], "phys": [], "draws": [], "objects": [], "prims": [], "nodes": []}
	var t_end := Time.get_ticks_usec() + int(secs * 1_000_000.0)
	var step := 0

	while Time.get_ticks_usec() < t_end:
		var a := Time.get_ticks_usec()
		if each.is_valid():
			each.call(step, (Time.get_ticks_usec() - (t_end - int(secs * 1_000_000.0))) / 1_000_000.0)
		await get_tree().process_frame
		var dt := (Time.get_ticks_usec() - a) / 1000.0
		step += 1
		if step <= 3:
			continue
		frames.append(dt)
		mons["proc"].append(Performance.get_monitor(Performance.TIME_PROCESS))
		mons["phys"].append(Performance.get_monitor(Performance.TIME_PHYSICS_PROCESS))
		mons["draws"].append(Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME))
		mons["objects"].append(Performance.get_monitor(Performance.RENDER_TOTAL_OBJECTS_IN_FRAME))
		mons["prims"].append(Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME))
		mons["nodes"].append(Performance.get_monitor(Performance.OBJECT_NODE_COUNT))
	mons["tweens"] = get_tree().get_processed_tweens().size() if get_tree().has_method("get_processed_tweens") else -1
	return {"frames": frames, "mons": mons}

func shot(tag: String) -> void:
	if OS.get_environment("JV_PROBE_SHOT") == "":
		return
	if DisplayServer.get_name() == "headless":
		p("screenshot skipped (headless) tag=%s" % tag)
		return
	await RenderingServer.frame_post_draw
	var out := OS.get_environment("JV_PROBE_SHOT").replace(".png", "-%s.png" % tag)
	get_viewport().get_texture().get_image().save_png(out)
	p("screenshot=%s" % out)

func _ready() -> void:
	var m: Node = null
	for i in 120:
		await get_tree().create_timer(1.0).timeout
		m = get_tree().current_scene
		if m == null: continue
		if m.has_method("_on_continue") and not m.get("_left"):
			p("boot scene %s -> continue" % m.scene_file_path); m.call("_on_continue"); continue
		if m.scene_file_path == "res://game/ui/title.tscn" and not m.get("_leaving"):
			p("title -> menu"); get_node("/root/GameConfig").goto_scene("res://game/ui/menu.tscn"); m.set("_leaving", true); continue
		if m.has_method("_card_btn") and m.has_method("_tilt_cards"):
			p("menu_ready path=%s file=%s injected=%s nodes=%d" % [m.get_path(), m.scene_file_path, m.has_meta("jvm_menu_ui"), get_tree().get_node_count()])
			await get_tree().create_timer(1.5).timeout
			break
	if m == null or not m.has_method("_tilt_cards"):
		p("no menu scene=%s" % (m.scene_file_path if m else "?"))
		get_tree().quit(3); return
	var mods := m.has_meta("jvm_menu_ui")
	var gate := get_node_or_null("/root/JVModGate")
	p("mods_injected=%s gate=%s menu_perf=%s" % [mods, gate != null, gate.get_node_or_null("JVMenuPerf") != null if gate else false])
	if "_tilt_fake" in m: m.set("_tilt_fake", 0.0)


	var r: Dictionary = await collect(3.0, Callable())
	summarize("static", r["frames"], r["mons"])
	await shot("static")


	r = await collect(3.0, func(step, t):
		if "_tilt_fake" in m: m.set("_tilt_fake", sin(t * 5.0)))
	if "_tilt_fake" in m: m.set("_tilt_fake", 0.0)
	summarize("tilt", r["frames"], r["mons"])
	await shot("tilt")


	var pages: Array = []
	for prop in ["_home", "_practice", "_local", "_settings", "_online"]:
		var pg = m.get(prop)
		if pg is Control: pages.append([prop, pg])
	p("pages_available=%s" % [pages.map(func(x): return x[0])])
	r = await collect(4.0, func(step, t):
		if pages.size() >= 2 and m.has_method("_show") and step % 20 == 0:
			m.call("_show", pages[(step / 20) % pages.size()][1]))
	summarize("pages", r["frames"], r["mons"])
	await shot("pages")
	if m.has_method("_show") and m.get("_home") is Control:
		m.call("_show", m.get("_home"))
	await get_tree().create_timer(0.5).timeout


	var cam = m.get("menu_cam")
	r = await collect(3.0, func(step, t):
		if cam is Node3D and cam.is_inside_tree():
			cam.global_position = cam.global_position + Vector3(sin(t * 4.0) * 0.08, 0.0, cos(t * 3.0) * 0.08))
	summarize("camera", r["frames"], r["mons"])


	var mp: Node = gate.get_node_or_null("JVMenuPerf") if gate else null
	if mp:
		p("menu_perf_info_after_on %s" % str(mp.get_info()))
		mp.reset_counters()
		mp.set_enabled(false)
		p("menu_perf toggled OFF")
		r = await collect(3.0, func(step, t):
			if "_tilt_fake" in m: m.set("_tilt_fake", sin(t * 5.0)))
		if "_tilt_fake" in m: m.set("_tilt_fake", 0.0)
		summarize("tilt_perf_off", r["frames"], r["mons"])
		await shot("tilt_perf_off")
		r = await collect(4.0, func(step, t):
			if pages.size() >= 2 and m.has_method("_show") and step % 20 == 0:
				m.call("_show", pages[(step / 20) % pages.size()][1]))
		summarize("pages_perf_off", r["frames"], r["mons"])
		await shot("pages_perf_off")
		if m.has_method("_show") and m.get("_home") is Control:
			m.call("_show", m.get("_home"))
		mp.set_enabled(true)
		p("menu_perf_info_after_off %s" % str(mp.get_info()))
	p("DONE mods=%s" % mods)
	get_tree().quit(0)
