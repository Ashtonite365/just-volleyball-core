#include "mod_gate.hpp"
#include "devtools.hpp"
#include "../common/jv_prof.hpp"
#include "menu_ui.hpp"
#include "menu_perf.hpp"
#include "mod_api.hpp"
#include "../common/jvm_common.hpp"
#include "version_check.hpp"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/e_net_multiplayer_peer.hpp>
#include <godot_cpp/classes/e_net_packet_peer.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>

#include <cstdio>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace godot;
using jvm::gate::Context;
using jvm::gate::Observation;
using jvm::gate::Visibility;

static std::string to_std(const String &s) { return std::string(s.utf8().get_data()); }
static const char *kLibs[] = { "core", "textures", "shots", "goofy", "models", "loader" };
static const char *kLibClass[] = { "JVModGate", "JVTextureLoader", "JVExtraShots", "JVGoofyMods", "JVModelLoader", "JVLoader" };
static constexpr int kLibCount = int(sizeof(kLibs) / sizeof(kLibs[0]));

JVModGate::JVModGate() {
	set_process_priority(-1000);
	set_process_mode(PROCESS_MODE_ALWAYS);
}

void JVModGate::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_active"), &JVModGate::is_active);
	ClassDB::bind_method(D_METHOD("is_cosmetic_active"), &JVModGate::is_cosmetic_active);
	ClassDB::bind_method(D_METHOD("get_reason"), &JVModGate::get_reason);
	ClassDB::bind_method(D_METHOD("get_cosmetic_reason"), &JVModGate::get_cosmetic_reason);
	ClassDB::bind_method(D_METHOD("get_mode"), &JVModGate::get_mode);
	ClassDB::bind_method(D_METHOD("get_observations"), &JVModGate::get_observations);
	ClassDB::bind_method(D_METHOD("verify_version"), &JVModGate::verify_version);
	ClassDB::bind_method(D_METHOD("pin_ok"), &JVModGate::pin_ok);
	ClassDB::bind_method(D_METHOD("get_pin_result"), &JVModGate::get_pin_result);
	ClassDB::bind_method(D_METHOD("register_library", "name", "state", "reason"), &JVModGate::register_library);
	ClassDB::bind_method(D_METHOD("api_library_allowed", "short_name"), &JVModGate::api_library_allowed);
	ClassDB::bind_method(D_METHOD("build_status"), &JVModGate::build_status);
	ClassDB::bind_method(D_METHOD("mark_status_dirty"), &JVModGate::mark_status_dirty);
	ClassDB::bind_method(D_METHOD("get_last_status_build_ms"), &JVModGate::get_last_status_build_ms);
	ClassDB::bind_method(D_METHOD("status_was_cached_build"), &JVModGate::status_was_cached_build);
	ClassDB::bind_method(D_METHOD("status_write_through"), &JVModGate::status_write_through);
	ClassDB::bind_method(D_METHOD("reload_config"), &JVModGate::reload_config);
	ClassDB::bind_method(D_METHOD("force_off", "why"), &JVModGate::force_off);
	ClassDB::bind_method(D_METHOD("set_overlay_visible", "on"), &JVModGate::set_overlay_visible);
	ADD_SIGNAL(MethodInfo("gate_changed", PropertyInfo(Variant::BOOL, "active"), PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("cosmetic_gate_changed", PropertyInfo(Variant::BOOL, "active"), PropertyInfo(Variant::STRING, "reason")));
}

double JVModGate::now_s() const { return double(Time::get_singleton()->get_ticks_usec()) / 1e6; }

String JVModGate::get_reason() const {
	if (!pin_ok_) return "disabled: " + String(pin_.get("reason", "game version not verified"));
	return String(sm_.reason().c_str());
}
String JVModGate::get_cosmetic_reason() const {
	if (!pin_ok_) return "disabled: " + String(pin_.get("reason", "game version not verified"));
	return String(sm_cos_.reason().c_str());
}

bool JVModGate::verify_version() {
	pin_ = jvm::verify_game_version();
	pin_ok_ = bool(pin_.get("ok", false));
	pin_checked_ = true;
	String msg = String("version pin: ") + (pin_ok_ ? "OK " : "MISMATCH ") + String(pin_.get("reason", "")) + " | found " + String(pin_.get("found", "")) +
			" | " + String::num_int64(int64_t(pin_.get("hash_ms", 0))) + " ms" + (bool(pin_.get("cached", false)) ? " (cached)" : " (hashed)");
	if (pin_ok_) jvm::log("pin", msg); else jvm::warn("pin", msg);
	return pin_ok_;
}

bool JVModGate::api_library_allowed(const String &short_name) {
	bool strict = false;
	const String p = jvm::mods_root().path_join("api.json");
	if (FileAccess::file_exists(p)) {
		Variant v = jvm::load_json(p);
		if (v.get_type() == Variant::DICTIONARY) strict = bool(Dictionary(v).get("strict", false));
	}
	std::string why;
	if (jvm::api::library_allowed(std::string(short_name.utf8().get_data()), strict, &why)) return true;
	jvm::log("api", String::utf8(why.c_str()));
	return false;
}

void JVModGate::register_library(const String &name, const String &state, const String &reason) {
	Dictionary d; d["state"] = state; d["reason"] = reason;
	libs_[name] = d;
	jvm::log("status", "library " + name + ": " + state + (reason.is_empty() ? "" : " (" + reason + ")"));
	mark_status_dirty();
}

void JVModGate::_ready() {
	if (!pin_checked_) verify_version();
	reload_config();
	ensure_ui();
	if (!pin_ok_) {
		banner_->set_text("just volleyball mods disabled: game version changed (expected build " + String(pin_.get("expected_build", "?")) +
				", found " + String(pin_.get("found", "?")) + "). Update the mods.");
		banner_->set_visible(true);
		banner_t_ = 20.0;
	}
	jvm::log("gate", "ready; mods OFF until an allowed context is positively identified");
	update_status(true);
	JVMenuUI *mu = memnew(JVMenuUI);
	mu->set_name("JVMenuUI");
	add_child(mu);
	JVMenuPerf *mp = memnew(JVMenuPerf);
	mp->set_name("JVMenuPerf");
	add_child(mp);
	JVModAPI *api = memnew(JVModAPI);
	api->set_name("JVModAPI");
	add_child(api);
	JVDevTools *dev = memnew(JVDevTools);
	dev->set_name("JVDevTools");
	add_child(dev);
}

void JVModGate::ensure_ui() {
	if (ui_) return;
	ui_ = memnew(CanvasLayer);
	ui_->set_layer(128);
	add_child(ui_);
	overlay_ = memnew(Label);
	overlay_->set_position(Vector2(12, 12));
	overlay_->add_theme_font_size_override("font_size", 14);
	overlay_->add_theme_color_override("font_color", Color(1, 1, 1));
	overlay_->add_theme_color_override("font_outline_color", Color(0, 0, 0));
	overlay_->add_theme_constant_override("outline_size", 4);
	overlay_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	overlay_->set_visible(false);
	ui_->add_child(overlay_);
	banner_ = memnew(Label);
	banner_->set_anchors_preset(Control::PRESET_TOP_WIDE);
	banner_->set_offset(SIDE_TOP, 40);
	banner_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	banner_->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	banner_->add_theme_font_size_override("font_size", 20);
	banner_->add_theme_color_override("font_color", Color(1, 0.85, 0.3));
	banner_->add_theme_color_override("font_outline_color", Color(0, 0, 0));
	banner_->add_theme_constant_override("outline_size", 6);
	banner_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	banner_->set_visible(false);
	ui_->add_child(banner_);
}

void JVModGate::set_overlay_visible(bool on) {
	overlay_on_ = on;
	if (overlay_) overlay_->set_visible(on);
	update_status(true);
}

void JVModGate::_input(const Ref<InputEvent> &event) {
	Ref<InputEventKey> k = event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo() && k->get_keycode() == KEY_F8) set_overlay_visible(!overlay_on_);
}

void JVModGate::reload_config() {
	OS *os = OS::get_singleton();
	String path = os->is_debug_build() ? os->get_environment("JV_GATE_CONFIG") : String();
	if (path.is_empty()) path = jvm::install_dir().path_join("gate_probes.json");
	Variant cfg = jvm::load_json(path);
	probes_.clear();
	if (cfg.get_type() == Variant::DICTIONARY) {
		Dictionary d = cfg;
		probes_ = d.get("probes", Array());
		policy_.max_age_s = double(d.get("max_age_s", 2.0));
		jvm::log("gate", "loaded " + String::num_int64(probes_.size()) + " probe(s) from " + path);
	} else {
		jvm::warn("gate", "no/invalid gate config at " + path + " -> no probes -> mods stay OFF");
	}
	String root = jvm::mods_root();
	kill_switch_ = FileAccess::file_exists(root.path_join("DISABLE"));
	Variant mj = jvm::load_json(root.path_join("jv_mods.json"));
	if (mj.get_type() == Variant::DICTIONARY && !bool(Dictionary(mj).get("enabled", true))) kill_switch_ = true;
	if (kill_switch_) jvm::log("gate", "kill switch set in " + root + " -> mods OFF");
}

void JVModGate::force_off(const String &why) {
	bool was = sm_.active(), was_c = sm_cos_.active();
	sm_.force_off(to_std(why));
	sm_cos_.force_off(to_std(why));
	if (was) emit_signal("gate_changed", false, why);
	if (was_c) emit_signal("cosmetic_gate_changed", false, why);
	if (was || was_c) jvm::log("gate", "FORCED OFF: " + why);
	update_status(true);
}

void JVModGate::_process(double delta) {
	JVM_PROF("core.gate_process");
	bool changed;
	{
		JVM_PROF("core.gate.evaluate");
		changed = evaluate();
	}
	if (banner_t_ > 0) {
		banner_t_ -= delta;
		if (banner_) banner_->set_modulate(Color(1, 1, 1, banner_t_ < 2.0 ? MAX(0.0, banner_t_ / 2.0) : 1.0));
		if (banner_t_ <= 0 && banner_) banner_->set_visible(false);
	}
	if (changed) status_followup_ = 3;
	update_status(changed);
}




static void strip_volatile_status(Dictionary &sig) {
	sig.erase("timestamp");
	sig.erase("timestamp_utc");
	sig.erase("unix_ms");
	sig.erase("frame");
	sig.erase("mode_changed_frame");
	Dictionary libs = sig.get("libraries", Dictionary());
	if (libs.has("models")) {
		Dictionary m = Dictionary(libs["models"]).duplicate(true);
		if (m.has("arms")) {
			Dictionary a = Dictionary(m["arms"]).duplicate(true);
			a.erase("max_stretch");
			m["arms"] = a;
			libs["models"] = m;
			sig["libraries"] = libs;
		}
	}
}

void JVModGate::stamp_volatile_status(Dictionary &st) {
	Time *tm = Time::get_singleton();
	st["timestamp"] = tm->get_datetime_string_from_system(false, false);
	st["timestamp_utc"] = tm->get_datetime_string_from_system(true, false) + "Z";
	st["unix_ms"] = int64_t(tm->get_unix_time_from_system() * 1000.0);
	st["frame"] = int64_t(Engine::get_singleton()->get_process_frames());
}

bool JVModGate::evaluate() {
	const double now = now_s();
	jvm::gate::Decision d, dc;
	if (!pin_ok_) {
		d = dc = { false, "blocked: game version not verified", "unknown" };
		mode_ = "unknown";
	} else if (kill_switch_) {
		d = dc = { false, "blocked: user kill switch", "unknown" };
		std::vector<Observation> obs = run_probes(now);
		mode_ = String(jvm::gate::mode_label(obs, now, policy_).c_str());
	} else {
		std::vector<Observation> obs = run_probes(now);
		d = jvm::gate::decide(obs, now, policy_, jvm::gate::Tier::Full);
		dc = jvm::gate::decide(obs, now, policy_, jvm::gate::Tier::Cosmetic);
		mode_ = String(d.mode.c_str());
	}
	const int64_t fr = int64_t(Engine::get_singleton()->get_process_frames());
	static thread_local String prev_mode;
	bool mode_ch = mode_ != prev_mode;
	if (mode_ch) { prev_mode = mode_; mode_changed_frame_ = fr; }
	bool ch = sm_.update(d, now);
	bool chc = sm_cos_.update(dc, now);
	if (ch) full_changed_frame_ = fr;
	if (chc) cos_changed_frame_ = fr;
	if (ch) {
		String r = String(sm_.reason().c_str());
		jvm::log("gate", String(sm_.active() ? "ON: " : "OFF: ") + r);
		last_logged_reason_ = r;
		emit_signal("gate_changed", sm_.active(), r);
	} else if (!sm_.active()) {
		String r = String(d.reason.c_str());
		if (r != last_logged_reason_) { last_logged_reason_ = r; jvm::log("gate", "OFF: " + r); }
	}
	if (chc) {
		String r = String(sm_cos_.reason().c_str());
		jvm::log("gate", String(sm_cos_.active() ? "cosmetic ON: " : "cosmetic OFF: ") + r);
		emit_signal("cosmetic_gate_changed", sm_cos_.active(), r);
	}
	return ch || chc || mode_ch;
}


Dictionary JVModGate::build_status() {
	Dictionary st;
	Time *t = Time::get_singleton();
	st["schema"] = 1;
	st["timestamp"] = t->get_datetime_string_from_system(false, false);
	st["timestamp_utc"] = t->get_datetime_string_from_system(true, false) + "Z";
	st["unix_ms"] = int64_t(t->get_unix_time_from_system() * 1000.0);
	st["frame"] = int64_t(Engine::get_singleton()->get_process_frames());
	st["mode"] = mode_;
	Dictionary gate, full, cos;
	full["active"] = is_active(); full["reason"] = get_reason(); full["changed_frame"] = full_changed_frame_;
	cos["active"] = is_cosmetic_active(); cos["reason"] = get_cosmetic_reason(); cos["changed_frame"] = cos_changed_frame_;
	st["mode_changed_frame"] = mode_changed_frame_;
	gate["full"] = full; gate["cosmetic"] = cos; gate["kill_switch"] = kill_switch_;
	st["gate"] = gate;
	Dictionary libs;
	for (int i = 0; i < kLibCount; i++) {
		String name = kLibs[i];
		Dictionary e;
		if (i == 0) {
			e["state"] = pin_ok_ ? "active" : "disabled-version-mismatch";
			e["reason"] = pin_ok_ ? String("gate running") : String("core only runs the version check, banner and status; ") + String(pin_.get("reason", ""));
		} else {
			Node *n = is_inside_tree() ? get_tree()->get_root()->get_node_or_null(NodePath(kLibClass[i])) : nullptr;
			if (n && n->has_method("get_status")) {
				static const int ids[] = { jvm::prof::stage("core.gate.status.lib.core"), jvm::prof::stage("core.gate.status.lib.textures"),
					jvm::prof::stage("core.gate.status.lib.shots"), jvm::prof::stage("core.gate.status.lib.goofy"),
					jvm::prof::stage("core.gate.status.lib.models"), jvm::prof::stage("core.gate.status.lib.loader") };
				static_assert(sizeof(ids) / sizeof(ids[0]) == kLibCount, "one profiler stage per library");
				jvm::prof::Scope sc(ids[i]);
				e = n->call("get_status");
			} else if (libs_.has(name)) {
				e = Dictionary(libs_[name]).duplicate();
			} else {
				e["state"] = "not-loaded"; e["reason"] = "library not registered";
			}
		}
		libs[name] = e;
	}
	st["libraries"] = libs;
	Node *tl = is_inside_tree() ? get_tree()->get_root()->get_node_or_null(NodePath("JVTextureLoader")) : nullptr;
	if (tl && tl->has_method("get_sync_status")) { JVM_PROF("core.gate.status.texture_sync"); st["texture_sync"] = tl->call("get_sync_status"); }
	else { Dictionary ts; ts["network"] = "off"; ts["reason"] = "texture library not loaded"; st["texture_sync"] = ts; }
	Dictionary pin;
	Array keys = pin_.keys();
	for (int i = 0; i < keys.size(); i++) pin[keys[i]] = pin_[keys[i]];
	st["version_pin"] = pin;
	if (JVModAPI *api = Object::cast_to<JVModAPI>(get_node_or_null(NodePath("JVModAPI")))) {
		Dictionary a;
		a["version"] = api->get_api_version();
		a["strict"] = api->is_strict();
		a["mods"] = api->get_mods().keys();
		a["coverage"] = api->get_coverage();
		st["api"] = a;
	}
	return st;
}

bool JVModGate::write_status_atomic(const String &json) {
	DirAccess::make_dir_recursive_absolute("user://jv_mods");
	String final_path = ProjectSettings::get_singleton()->globalize_path("user://jv_mods/status.json");
	String tmp_path = final_path + ".tmp";
	{
		Ref<FileAccess> f = FileAccess::open(tmp_path, FileAccess::WRITE);
		if (f.is_null()) return false;
		f->store_string(json);
		f->flush();
	}
#ifdef _WIN32







	std::wstring a((const wchar_t *)tmp_path.utf16().get_data()), b((const wchar_t *)final_path.utf16().get_data());
	return MoveFileExW(a.c_str(), b.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
	return std::rename(tmp_path.utf8().get_data(), final_path.utf8().get_data()) == 0;
#endif
}

void JVModGate::update_status(bool force) {
	const double now = now_s();
	if (status_followup_ > 0) status_followup_--;




	if (!force && status_followup_ <= 0 && !overlay_on_ && !status_content_dirty_ && now - last_write_ < 0.5) return;

	JVM_PROF("core.gate.status");
	const uint64_t t0 = Time::get_singleton()->get_ticks_usec();
	Dictionary st;



	if (!force && status_followup_ <= 0 && !status_content_dirty_ && !last_built_status_.is_empty()) {
		st = last_built_status_.duplicate(true);
		stamp_volatile_status(st);
		last_status_cached_ = true;
	} else {
		JVM_PROF("core.gate.status.build");
		st = build_status();
		last_built_status_ = st.duplicate(true);
		status_content_dirty_ = false;
		last_status_cached_ = false;
	}
	last_status_build_ms_ = double(Time::get_singleton()->get_ticks_usec() - t0) / 1000.0;
	String s;
	{
		JVM_PROF("core.gate.status.signature");
		Dictionary sig = st.duplicate(true);
		strip_volatile_status(sig);
		s = JSON::stringify(sig, "", true);
	}
	bool changed = s != last_sig_;
	if (!changed && !force && now - last_write_ < 0.5 && !overlay_on_) return;
	if (changed && !last_sig_.is_empty()) {
		Dictionary libs = st["libraries"];
		String line = "status: mode=" + String(st["mode"]) + " full=" + String(Dictionary(Dictionary(st["gate"])["full"])["active"]) +
				" cosmetic=" + String(Dictionary(Dictionary(st["gate"])["cosmetic"])["active"]);
		for (int i = 1; i < kLibCount; i++) line += String(" ") + kLibs[i] + "=" + String(Dictionary(libs[kLibs[i]])["state"]);
		line += " sync=" + String(Dictionary(st["texture_sync"]).get("network", "off"));
		line += " | " + String(Dictionary(Dictionary(st["gate"])["full"])["reason"]);
		jvm::log("status", line);
	}



	if (force || changed || now - last_write_ >= 0.5) {
		bool wrote;
		{
			JVM_PROF("core.gate.status.write");
			wrote = write_status_atomic(JSON::stringify(st, "  ", true));
		}
		if (wrote) {
			last_sig_ = s;
			last_write_ = now;
			write_fail_ = 0;
		} else if (++write_fail_ == 1) {
			jvm::warn("status", "could not write status.json (will retry)");
		}
	} else if (changed) {
		last_sig_ = s;
	}
	if (overlay_ && overlay_on_) {
		String txt = "jv-mods (F8)  mode: " + String(st["mode"]) + "\n";
		txt += "full: " + String(is_active() ? "ON" : "off") + "  - " + get_reason() + "\n";
		txt += "cosmetic: " + String(is_cosmetic_active() ? "ON" : "off") + "  - " + get_cosmetic_reason() + "\n";
		Dictionary libs = st["libraries"];
		for (int i = 0; i < kLibCount; i++) { Dictionary e = libs[kLibs[i]]; txt += String(kLibs[i]) + ": " + String(e.get("state", "?")) + "\n"; }
		Dictionary ts = st["texture_sync"];
		txt += "texture sync: " + String(ts.get("network", "off")) + " " + String(ts.get("channel", "")) + "\n";
		txt += "version: " + String(pin_ok_ ? "OK " : "MISMATCH ") + String(pin_.get("matched", "")) + "\n" + String(st["timestamp"]);
		overlay_->set_text(txt);
	}
}


void JVModGate::apply_role(const Dictionary &spec, const String &raw_in, Observation &o) {
	String raw = raw_in;
	if (spec.has("map")) {
		Dictionary m = spec["map"];
		raw = m.has(raw_in) ? String(m[raw_in]) : String();
	}
	String role = spec.get("role", "context");
	if (role == "context") {
		o.context = jvm::gate::context_from_string(to_std(raw));
		if (o.context == Context::OfflinePlay) {
			String l = raw.to_lower().replace(" ", "_").replace("-", "_");
			o.detail = to_std(l == "freeplay" ? String("free_play") : (l == "solo" ? String("offline_solo") : l));
		}
	} else if (role == "visibility") o.visibility = jvm::gate::visibility_from_string(to_std(raw));
	else if (role == "password") o.password_protected = (raw == "true" || raw == "1");
}

std::vector<Observation> JVModGate::run_probes(double now) {
	JVM_PROF("core.gate.evaluate.probes");
	std::vector<Observation> out;
	last_obs_.clear();
	for (int i = 0; i < probes_.size(); i++) {
		if (probes_[i].get_type() != Variant::DICTIONARY) continue;
		Dictionary spec = probes_[i];
		Observation o;
		o.source = to_std(String(spec.get("name", spec.get("type", "probe"))));
		o.authoritative = bool(spec.get("authoritative", false));
		o.timestamp = now;
		String type = spec.get("type", "");
		bool ok = false;
		if (type == "node_property") ok = probe_node_property(spec, o);
		else if (type == "steam_lobby_data") ok = probe_steam_lobby_data(spec, o);
		else if (type == "multiplayer_peer") { o.authoritative = false; ok = probe_multiplayer_peer(spec, o); }
		else if (type == "test") ok = probe_test(spec, o);
		else if (type == "jv_net") ok = probe_jv_net(spec, o);
		if (!ok) continue;
		out.push_back(o);
		Dictionary dd;
		dd["source"] = String(o.source.c_str());
		dd["context"] = String(jvm::gate::to_string(o.context));
		dd["visibility"] = String(jvm::gate::to_string(o.visibility));
		dd["detail"] = String(o.detail.c_str());
		dd["authoritative"] = o.authoritative;
		last_obs_.push_back(dd);
	}
	return out;
}

bool JVModGate::probe_node_property(const Dictionary &spec, Observation &o) {
	if (!is_inside_tree()) return false;
	Node *n = get_tree()->get_root()->get_node_or_null(NodePath(String(spec.get("node", ""))));
	if (!n) return false;
	String prop = spec.get("property", "");
	if (prop.is_empty()) return false;
	Variant v = n->get_indexed(NodePath(prop));
	if (v.get_type() == Variant::NIL) return false;
	apply_role(spec, v.stringify(), o);
	return true;
}

bool JVModGate::probe_steam_lobby_data(const Dictionary &spec, Observation &o) {
	Engine *e = Engine::get_singleton();
	if (!e->has_singleton("Steam")) return false;
	Object *steam = e->get_singleton("Steam");
	Dictionary lid = spec.get("lobby_id", Dictionary());
	Node *n = is_inside_tree() ? get_tree()->get_root()->get_node_or_null(NodePath(String(lid.get("node", "")))) : nullptr;
	if (!n) return false;
	Variant id = n->get_indexed(NodePath(String(lid.get("property", ""))));
	if (id.get_type() != Variant::INT || int64_t(id) == 0) return false;
	if (!steam->has_method("getLobbyData")) return false;
	String raw = steam->call("getLobbyData", id, String(spec.get("key", "")));
	if (raw.is_empty()) return false;
	apply_role(spec, raw, o);
	return true;
}

bool JVModGate::probe_multiplayer_peer(const Dictionary &, Observation &o) {
	if (!is_inside_tree()) return false;
	Ref<MultiplayerAPI> mp = get_tree()->get_multiplayer();
	if (mp.is_null()) return false;
	Ref<MultiplayerPeer> peer = mp->get_multiplayer_peer();
	if (peer.is_null() || peer->get_class() == "OfflineMultiplayerPeer") return false;
	if (peer->get_connection_status() != MultiplayerPeer::CONNECTION_CONNECTED) return false;
	Ref<ENetMultiplayerPeer> en = peer;
	if (en.is_null()) { o.context = Context::Online; return true; }
	PackedInt32Array ids = mp->get_peers();
	if (ids.is_empty()) return false;
	for (int i = 0; i < ids.size(); i++) {
		Ref<ENetPacketPeer> pp = en->get_peer(ids[i]);
		if (pp.is_null()) return false;
		if (!jvm::gate::is_lan_address(to_std(pp->get_remote_address()))) { o.context = Context::Online; return true; }
	}
	o.context = Context::Lan;
	return true;
}

bool JVModGate::probe_test(const Dictionary &spec, Observation &o) {
	if (!OS::get_singleton()->is_debug_build()) return false;
	Dictionary s = spec;
	if (spec.has("file")) {
		Variant v = jvm::load_json(spec["file"]);
		if (v.get_type() != Variant::DICTIONARY) return false;
		s = v;
	}
	o.context = jvm::gate::context_from_string(to_std(String(s.get("context", ""))));
	o.visibility = jvm::gate::visibility_from_string(to_std(String(s.get("visibility", ""))));
	o.password_protected = bool(s.get("password", false));
	o.detail = to_std(String(s.get("detail", "")));
	return true;
}



bool JVModGate::probe_jv_net(const Dictionary &spec, Observation &o) {
	if (!is_inside_tree()) return false;
	SceneTree *tree = get_tree();
	Node *root = tree->get_root();
	Node *net = root->get_node_or_null(NodePath(String(spec.get("net", "/root/Net"))));
	Node *gc = root->get_node_or_null(NodePath(String(spec.get("game_config", "/root/GameConfig"))));
	if (!net || !gc) return false;
	bool ok = true;
	auto prop = [&ok](Object *obj, const char *name) -> Variant {
		Variant v = obj->get(StringName(name));
		if (v.get_type() == Variant::NIL) ok = false;
		return v;
	};
	Variant quick = prop(gc, "quick_match");
	Variant net_roster = prop(gc, "net_roster");
	Variant active = prop(net, "active");
	Variant custom = prop(net, "custom_lobby");
	Variant party_lan = prop(net, "party_lan");
	Variant kind = prop(net, "_lobby_kind");
	Variant privacy = prop(net, "privacy");
	Variant password = prop(net, "password");
	Variant lobby_id = prop(net, "steam_lobby_id");
	if (!ok) return false;

	Node *scene = tree->get_current_scene();
	String scene_path = scene ? scene->get_scene_file_path() : String();
	const String menu_scene = spec.get("menu_scene", "res://game/ui/menu.tscn");
	const String main_scene = spec.get("main_scene", "res://game/main/main.tscn");
	const bool in_menu = scene && scene_path == menu_scene;





	const Variant ranked = gc->get(StringName("ranked_match"));
	if (ranked.get_type() == Variant::BOOL && bool(ranked)) { o.context = Context::QuickMatch; o.detail = "ranked"; return true; }
	const String k = kind;
	if (bool(quick) || k == "locked" || k == "quick") { o.context = Context::QuickMatch; return true; }
	if (in_menu) {

		Variant mq = prop(scene, "_member_queuing");
		if (!ok) return false;
		Variant qp = scene->get("_qp");
		if (bool(mq)) { o.context = Context::QuickMatch; return true; }
		if (qp.get_type() == Variant::OBJECT) {
			Object *q = qp;
			if (q && int64_t(q->get("state")) != 0) {
				o.context = Context::QuickMatch;
				if (bool(q->get("ranked"))) o.detail = "ranked";
				return true;
			}
		}
	}

	if (!bool(active)) {


		Ref<MultiplayerPeer> peer = tree->get_multiplayer()->get_multiplayer_peer();
		bool has_peer = peer.is_valid() && peer->get_class() != "OfflineMultiplayerPeer";
		if (has_peer || int64_t(lobby_id) != 0) { o.context = Context::Unknown; return true; }
		if (in_menu) {
			Variant solo = prop(scene, "_practice_solo");
			Variant pon = prop(scene, "_practice_on");
			Variant fp = prop(scene, "_freeplay");
			Variant drill = prop(scene, "_drill_active");
			if (!ok) return false;
			if (!String(drill).is_empty()) { o.context = Context::OfflinePlay; o.detail = "practice"; return true; }
			if (bool(solo) || bool(pon) || bool(fp)) { o.context = Context::OfflinePlay; o.detail = "free_play"; return true; }
			o.context = Context::Offline;
			return true;
		}
		if (scene && scene_path == main_scene) {
			if (Array(net_roster).is_empty()) { o.context = Context::OfflinePlay; o.detail = "offline_solo"; return true; }
			o.context = Context::Unknown;
			return true;
		}
		o.context = Context::Offline;
		return true;
	}

	Ref<MultiplayerPeer> peer = tree->get_multiplayer()->get_multiplayer_peer();
	Ref<ENetMultiplayerPeer> en = peer;
	if (bool(party_lan) || en.is_valid()) { o.context = Context::Lan; return true; }
	if (!bool(custom)) { o.context = Context::Unknown; return true; }
	o.context = Context::CustomLobby;
	switch (int64_t(privacy)) {
		case 0: o.visibility = Visibility::Public; break;
		case 1: o.visibility = Visibility::FriendsOnly; break;
		case 2: o.visibility = Visibility::Private; o.password_protected = !String(password).is_empty(); break;
		default: o.visibility = Visibility::Unknown; break;
	}
	Engine *e = Engine::get_singleton();
	if (e->has_singleton("Steam") && int64_t(lobby_id) != 0) {
		Object *steam = e->get_singleton("Steam");
		if (steam->has_method("getLobbyData")) {
			String priv = steam->call("getLobbyData", lobby_id, "priv");
			Visibility sv = Visibility::Unknown;
			if (priv == "public") sv = Visibility::Public;
			else if (priv == "friends") sv = Visibility::FriendsOnly;
			else if (priv == "password" || priv == "invite") sv = Visibility::Private;
			if (sv == Visibility::Public) o.visibility = Visibility::Public;
			else if (sv != Visibility::Unknown && sv != o.visibility) o.visibility = Visibility::Unknown;
		}
	}
	return true;
}
