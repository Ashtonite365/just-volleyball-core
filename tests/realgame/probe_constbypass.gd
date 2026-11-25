extends Node


const PG := "res://game/player/player.gd"
const PT := "res://game/player/player.tscn"
func p(m: String) -> void: print("PROBE ", m)

func _ready() -> void:
	p("engine=%s" % Engine.get_version_info().string)
	p("at_probe cached player.gd=%s player.tscn=%s main.gd=%s" % [ResourceLoader.has_cached(PG), ResourceLoader.has_cached(PT), ResourceLoader.has_cached("res://game/main/main.gd")])
	var f := FileAccess.open("res://game/player/player.gdc", FileAccess.READ)
	if f: var h := f.get_buffer(8); p("gdc_readable=true len=%d magic=%s ver=%d" % [f.get_length(), h.slice(0, 4).get_string_from_ascii(), h.decode_u32(4)])
	else: p("gdc_readable=false err=%d" % FileAccess.get_open_error())
	var P: GDScript = load(PG)
	p("P has_source=%s srclen=%d const_DIVE_DENY_TIME=%s const_count=%d" % [P.has_source_code(), P.source_code.length(), P.get_script_constant_map().get("DIVE_DENY_TIME"), P.get_script_constant_map().size()])
	var cm := P.get_script_constant_map(); cm["DIVE_DENY_TIME"] = 9.0
	p("constmap_is_copy=%s" % [P.get_script_constant_map().get("DIVE_DENY_TIME") == 0.35])
	P.set("DIVE_DENY_TIME", 9.0)
	p("P.set(const) took=%s get=%s" % [P.get("DIVE_DENY_TIME") == 9.0, P.get("DIVE_DENY_TIME")])

	var S0: GDScript = load("res://mods/cb/_s.gd")
	var s0 = S0.new()
	p("synthetic g()=%s (2=instance override honoured from base code) sg()=%s (3=static override honoured)" % [s0.g(), s0.sg()])
	_option_b(P)
	_option_a(P)
	_overlay_pck()
	await get_tree().create_timer(6.0).timeout
	p("after_boot_6s alive=true players_in_tree=%d" % get_tree().root.find_children("*", "CharacterBody3D", true, false).size())
	p("DONE")
	get_tree().quit()

func _option_b(P: GDScript) -> void:
	var S := GDScript.new()
	S.source_code = """extends "res://game/player/player.gd"
static var OV := {}
func in_front_zone() -> bool:
	if OV.has("FRONT_ZONE_GRACE"): return OV["FRONT_ZONE_GRACE"] > 100.0
	return super()
func set_dump_rise(power: float) -> float:
	if OV.has("SET_DUMP_RISE_TAP"): return lerpf(OV["SET_DUMP_RISE_TAP"], 0.2, clampf(power, 0.0, 1.0))
	return super(power)
static func spike_turn_keep(turn: float) -> float:
	if OV.has("SPIKE_TURN_FLOOR"): return OV["SPIKE_TURN_FLOOR"]
	return 1.0
"""
	var t0 := Time.get_ticks_usec(); var err := S.reload()
	p("B subclass compile err=%d us=%d" % [err, Time.get_ticks_usec() - t0])
	if err != OK: return
	var T: PackedScene = load(PT)
	var b: Dictionary = T.get("_bundled")
	var vars: Array = b["variants"]; var idx := vars.find(P)
	p("B tscn variants=%d player_script_idx=%d" % [vars.size(), idx])
	if idx < 0: return
	vars[idx] = S; b["variants"] = vars; T.set("_bundled", b)
	var n = T.instantiate()
	p("B swapped instance script_is_S=%s is_Player=%s script_path='%s' FEEL_ok=%s" % [n.get_script() == S, n is Player, n.get_script().resource_path, n.get("FEEL") != null])
	S.set("OV", {"SET_DUMP_RISE_TAP": 5.0, "FRONT_ZONE_GRACE": 999.0, "SPIKE_TURN_FLOOR": 0.77})
	p("B set_dump_rise(0)=%s (stock 2.8, override 5.0)" % n.set_dump_rise(0.0))
	p("B set_dump_intent(true)->in_front_zone via base code=%s (override true)" % n.set_dump_intent(true))
	p("B spike_turn_keep via instance=%s via P=%s" % [n.spike_turn_keep(3.0), P.spike_turn_keep(3.0)])
	n.free()
	vars[idx] = P; b["variants"] = vars; T.set("_bundled", b)
	var n2 = T.instantiate()
	p("B revert script_is_P=%s set_dump_rise(0)=%s" % [n2.get_script() == P, n2.set_dump_rise(0.0)])
	n2.free()

func _option_a(P: GDScript) -> void:
	var src := FileAccess.get_file_as_string("res://mods/cb/_player_patched.txt")
	if src.is_empty(): p("A fixture missing"); return
	var T: PackedScene = load(PT)
	var held = T.instantiate()
	var e0 := P.reload(false)
	p("A reload(false) with live instance err=%d" % e0)
	held.free()
	P.source_code = src
	p("A after set: has_source=%s srclen=%d" % [P.has_source_code(), P.source_code.length()])
	var t0 := Time.get_ticks_usec(); var err := P.reload(false)
	p("A reload(false) no instances err=%d const_gone=%s" % [err, not P.get_script_constant_map().has("DIVE_DENY_TIME")])
	if P.get_script_constant_map().has("DIVE_DENY_TIME"):
		held = T.instantiate()
		err = P.reload(true)
		p("A reload(true) err=%d const_gone=%s" % [err, not P.get_script_constant_map().has("DIVE_DENY_TIME")])
		held.free()
	held = T.instantiate()
	p("A in-place reload(keep_state) err=%d ms=%d same_obj=%s still_global=%s" % [err, (Time.get_ticks_usec() - t0) / 1000, load(PG) == P, ResourceLoader.load(PG) == P])
	if err != OK: held.free(); return
	p("A const_gone=%s static DIVE_DENY_TIME=%s SET_DUMP_RISE_TAP=%s" % [not P.get_script_constant_map().has("DIVE_DENY_TIME"), P.get("DIVE_DENY_TIME"), P.get("SET_DUMP_RISE_TAP")])
	p("A held instance still works set_dump_rise(0)=%s" % held.set_dump_rise(0.0))
	held.free()
	var n = T.instantiate()
	p("A new instance is_Player=%s set_dump_rise(0)=%s" % [n is Player, n.set_dump_rise(0.0)])
	P.set("SET_DUMP_RISE_TAP", 5.0); P.set("DIVE_DENY_TIME", 1.25)
	n.dive_denied()
	p("A live edit set_dump_rise(0)=%s (want 5) _dive_deny_t=%s (want 1.25) spike_turn_keep(3)=%s" % [n.set_dump_rise(0.0), n.get("_dive_deny_t"), P.spike_turn_keep(3.0)])
	P.set("SPIKE_TURN_FLOOR", 0.5)
	p("A static func sees static var spike_turn_keep(3)=%s (want 0.5)" % P.spike_turn_keep(3.0))

	P.set("SET_DUMP_RISE_TAP", 2.8); P.set("DIVE_DENY_TIME", 0.35); P.set("SPIKE_TURN_FLOOR", 0.15)
	p("A revert set_dump_rise(0)=%s" % n.set_dump_rise(0.0))
	n.free()

func _overlay_pck() -> void:
	var pk := PCKPacker.new()
	var tmp := OS.get_user_data_dir().path_join("cb_overlay.txt")
	var fw := FileAccess.open(tmp, FileAccess.WRITE); fw.store_string("overlay-ok"); fw.close()
	var pck := OS.get_user_data_dir().path_join("cb_overlay.pck")
	var e1 := pk.pck_start(pck); var e2 := pk.add_file("res://mods/cb/overlay_probe.txt", tmp); var e3 := pk.flush()
	var ok := ProjectSettings.load_resource_pack(pck, true)
	p("overlay pck start=%d add=%d flush=%d mounted=%s read=%s" % [e1, e2, e3, ok, FileAccess.get_file_as_string("res://mods/cb/overlay_probe.txt")])
