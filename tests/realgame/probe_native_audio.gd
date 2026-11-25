extends Node


const M := "jv_audio_probe"
const BEACH_PATH := "res://game/sfx/Ambiance/717146__ghosttropic__short-sand-beach-waves-3.wav"
const BEACH_ID := "ambiance/717146__ghosttropic__short-sand-beach-waves-3"
func p(m: String) -> void: print("PROBE ", m)

func idle_ok(st: Dictionary) -> bool:
	return (not bool(st.get("started", true))) and int(st.get("players", 1)) == 0 \
		and int(st.get("nodes", 1)) == 0 and int(st.get("cached_streams", 1)) == 0 \
		and int(st.get("cached_stream_refs", 1)) == 0

func _ready() -> void:

	var a: Object = null
	for i in 90:
		await get_tree().create_timer(1.0).timeout
		if Engine.has_singleton("JVModAPI"):
			a = Engine.get_singleton("JVModAPI")
		var tex: Node = get_node_or_null("/root/JVTextureLoader")
		if a != null and tex != null:
			break
		if i % 10 == 9: p("waiting boot api=%s loader=%s scene=%s" % [a != null, tex != null, get_tree().current_scene])
	p("api=%s version=%s loader=%s" % [a != null, a.get_api_version() if a else "-", get_node_or_null("/root/JVTextureLoader") != null])
	if a == null:
		p("FAIL no API"); get_tree().quit(4); return
	p("api_version=%s" % a.get_api_version())
	var r: Dictionary = a.register_mod(M, "1.4", PackedStringArray(["audio.query", "audio.play", "packs.query", "stage.pick", "court.query", "court.env"]))
	p("register ok=%s denied=%s" % [r.get("ok"), str(r.get("denied"))])

	var st0: Dictionary = a.audio_status(M)
	p("idle_before=%s started=%s players=%s nodes=%s streams=%s index_built=%s allocations=%s" % [
		idle_ok(st0), st0.get("started"), st0.get("players"), st0.get("nodes"),
		st0.get("cached_streams"), st0.get("index_built"), st0.get("allocations")])
	if not idle_ok(st0): p("FAIL system was not idle at boot")

	var buses: PackedStringArray = a.audio_buses(M)
	p("buses n=%d names=%s" % [buses.size(), ",".join(buses)])
	p("buses_still_idle=%s" % idle_ok(a.audio_status(M)))

	var lst: Array = a.audio_list(M)
	var st1: Dictionary = a.audio_status(M)
	p("list n=%d index_count=%s index_built=%s started=%s players=%s nodes=%s streams=%s" % [
		lst.size(), st1.get("index_count"), st1.get("index_built"), st1.get("started"),
		st1.get("players"), st1.get("nodes"), st1.get("cached_streams")])
	p("list_allocates_no_players=%s" % (idle_ok({
		"started": st1.get("started"), "players": st1.get("players"),
		"nodes": st1.get("nodes"), "cached_streams": st1.get("cached_streams"),
		"cached_stream_refs": st1.get("cached_stream_refs", 0)
	})))

	var beach_ids := []
	var beach_entry := {}
	for e in lst:
		var path := String(e.get("path", ""))
		var id := String(e.get("id", ""))
		if path == BEACH_PATH or id == BEACH_ID or id.ends_with("short-sand-beach-waves-3"):
			beach_entry = e
			beach_ids.append("%s|%s|%s" % [id, path, e.get("bus")])
	p("beach_found=%s id=%s path=%s bus=%s aliases_ok=%s" % [
		not beach_entry.is_empty(), beach_entry.get("id"), beach_entry.get("path"),
		beach_entry.get("bus"), not a.audio_info(M, "beach").is_empty() and bool(a.audio_info(M, "beach").get("found"))])
	p("beach_info=%s" % str(a.audio_info(M, "beach")))

	var examples := []
	for e in lst:
		var id := String(e.get("id"))
		if id.begins_with("ambiance/") or id.begins_with("announcer/") or id.begins_with("ost/"):
			examples.append(id)
		if examples.size() >= 8: break
	p("example_ids=%s" % ",".join(examples))


	a.packs_rescan(M)
	await get_tree().create_timer(0.4).timeout
	var info: Dictionary = a.pack_info(M, "10F8")
	var ei: Dictionary = a.court_env_info(M, "10F8")
	p("pack_10F8 found=%s has_audio=%s audio=%s court_env_has_audio=%s" % [
		not info.is_empty(), info.get("has_audio"), str(info.get("audio")), ei.get("has_audio")])

	var h: int = int(a.audio_apply_pack(M, "10F8"))
	if h == 0:
		h = int(a.audio_play_loop(M, "beach", {"bus": "ambient"}))
		p("apply_pack_handle=0 fallback_play_loop=%d" % h)
	else:
		p("apply_pack_handle=%d" % h)
	await get_tree().create_timer(0.8).timeout
	var st2: Dictionary = a.audio_status(M)
	p("playing started=%s players=%s nodes=%s streams=%s pack_handle=%s host=%s idle=%s" % [
		st2.get("started"), st2.get("players"), st2.get("nodes"), st2.get("cached_streams"),
		st2.get("pack_handle"), st2.get("host"), st2.get("idle")])

	var h2: int = int(a.audio_play(M, "announcer/ace"))
	p("oneshot_ace=%d" % h2)
	await get_tree().create_timer(0.2).timeout
	p("after_oneshot players=%s streams=%s" % [a.audio_status(M).get("players"), a.audio_status(M).get("cached_streams")])
	a.audio_stop_all(M)
	await get_tree().create_timer(0.2).timeout
	var st3: Dictionary = a.audio_status(M)
	p("after_stop idle=%s started=%s players=%s nodes=%s streams=%s index_kept=%s" % [
		idle_ok(st3), st3.get("started"), st3.get("players"), st3.get("nodes"),
		st3.get("cached_streams"), st3.get("index_count")])
	p("DONE")
	get_tree().quit(0)
