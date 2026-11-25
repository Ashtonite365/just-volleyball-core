






#include "mod_api.hpp"
#include "mod_gate.hpp"
#include "devtools.hpp"
#include "../common/jvm_common.hpp"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;
namespace A = jvm::api;

static String G(const std::string &s) { return String::utf8(s.c_str()); }
static std::string S(const String &s) { return std::string(s.utf8().get_data()); }
static Array args() { return Array(); }
template <typename... T> static Array args(const T &...v) { Array a; (a.push_back(Variant(v)), ...); return a; }


Node *JVModAPI::lib(const char *which) const {
	if (!is_inside_tree()) return nullptr;
	const char *p = A::library_path(which);
	if (!*p) return nullptr;
	return get_tree()->get_root()->get_node_or_null(NodePath(p));
}



Variant JVModAPI::fwd(const String &mod, const char *cap, const char *which, const char *method, const Array &a, const Variant &dflt, bool need_open) {
	if (!use(mod, cap, need_open)) return dflt;
	Node *n = lib(which);
	if (!n || !n->has_method(method)) return dflt;
	Variant r = n->callv(method, a);
	return r.get_type() == Variant::NIL && dflt.get_type() != Variant::NIL ? dflt : r;
}
static Dictionary D(const Variant &v) { return v.get_type() == Variant::DICTIONARY ? Dictionary(v) : Dictionary(); }
static Array AR(const Variant &v) { return v.get_type() == Variant::ARRAY ? Array(v) : Array(); }
static bool B(const Variant &v) { return v.get_type() == Variant::NIL ? false : bool(v); }
static String STR(const Variant &v) { return v.get_type() == Variant::NIL ? String() : String(v); }


Dictionary JVModAPI::gate_matrix() const {

	Dictionary out;
	static const char *acc[] = { "read", "local_ui", "cosmetic", "gameplay", "sync", "offline" };
	for (auto &m : A::mode_labels()) {
		A::GateView g;
		g.pin_ok = true; g.mode = m;
		g.cosmetic = m != "unknown";
		g.full = !(m == "public" || m == "quick match" || m == "server browser" || m == "unknown");
		Dictionary row;
		for (const char *ac : acc) {
			A::Access a = A::Access::Read;
			for (int i = 0; i < 6; i++) if (String(acc[i]) == ac) a = A::Access(i);
			row[ac] = A::access_open(a, g);
		}
		out[G(m)] = row;
	}
	return out;
}
Dictionary JVModAPI::net_safety(const String &cap) const {
	Dictionary d;
	const A::Capability *c = A::find(S(cap));
	d["found"] = c != nullptr;
	if (!c) return d;
	d["access"] = A::to_string(c->access);
	d["scope"] = A::to_string(c->scope);
	PackedStringArray open_in;
	Dictionary m = gate_matrix();
	Array keys = m.keys();
	for (int i = 0; i < keys.size(); i++) if (bool(Dictionary(m[keys[i]])[A::to_string(c->access)])) open_in.push_back(keys[i]);
	d["open_in"] = open_in;
	d["public_safe"] = open_in.has("public");
	d["reaches_other_players"] = c->scope != A::Scope::Local;
	return d;
}


Dictionary JVModAPI::libraries_status(const String &mod) {
	Dictionary out;
	if (!use(mod, "lib.status")) return out;
	static const char *libs[] = { "loader", "textures", "models", "shots", "goofy", "menu_ui", "menu_perf", "picker" };
	for (const char *l : libs) {
		Node *n = lib(l);
		Dictionary e;
		e["present"] = n != nullptr;
		e["has_status"] = n && n->has_method("get_status");
		out[l] = e;
	}
	if (JVModGate *g = Object::cast_to<JVModGate>(get_parent())) {
		Dictionary st = g->call("build_status");
		out["gate_libraries"] = st.get("libraries", Variant());
	}
	return out;
}
Dictionary JVModAPI::library_status(const String &mod, const String &library) {
	if (!use(mod, "lib.status")) return Dictionary();
	Node *n = lib(S(library).c_str());
	if (!n) return Dictionary();
	const char *m = n->has_method("get_status") ? "get_status" : (n->has_method("get_info") ? "get_info" : nullptr);
	return m ? D(n->call(m)) : Dictionary();
}
bool JVModAPI::library_reload(const String &mod, const String &library) {
	if (!use(mod, "lib.reload")) return false;
	Node *n = lib(S(library).c_str());
	if (!n || !n->has_method("reload_config")) return false;
	return B(n->call("reload_config"));
}

String JVModAPI::state_path(const String &mod) const {
	String safe = mod.validate_filename().replace(" ", "_");
	return jvm::mods_root().path_join("api").path_join("state").path_join(safe + String(".json"));
}
static Dictionary read_state(const String &p) {
	if (!FileAccess::file_exists(p)) return Dictionary();
	return D(jvm::load_json(p));
}
static bool write_state(const String &p, const Dictionary &d) {
	DirAccess::make_dir_recursive_absolute(p.get_base_dir());
	Ref<FileAccess> f = FileAccess::open(p + String(".tmp"), FileAccess::WRITE);
	if (f.is_null()) return false;
	f->store_string(JSON::stringify(d, "  "));
	f->close();
	return DirAccess::rename_absolute(p + String(".tmp"), p) == OK;
}
Variant JVModAPI::state_get(const String &mod, const String &key, const Variant &dflt) {
	if (!use(mod, "config.state")) return dflt;
	return read_state(state_path(mod)).get(key, dflt);
}
bool JVModAPI::state_set(const String &mod, const String &key, const Variant &value) {
	if (!use(mod, "config.state") || key.is_empty()) return false;
	String p = state_path(mod);
	Dictionary d = read_state(p);
	d[key] = value;
	return write_state(p, d);
}
bool JVModAPI::state_erase(const String &mod, const String &key) {
	if (!use(mod, "config.state", false)) return false;
	String p = state_path(mod);
	Dictionary d = read_state(p);
	if (!d.has(key)) return false;
	d.erase(key);
	return write_state(p, d);
}
PackedStringArray JVModAPI::state_keys(const String &mod) {
	PackedStringArray out;
	if (!use(mod, "config.state")) return out;
	Array k = read_state(state_path(mod)).keys();
	for (int i = 0; i < k.size(); i++) out.push_back(k[i]);
	return out;
}
Dictionary JVModAPI::config_validate(const String &mod, const String &library, const Dictionary &config) {
	Dictionary d;
	if (!use(mod, "config.validate")) { d["ok"] = false; d["errors"] = args("refused"); return d; }
	Node *n = lib(S(library).c_str());
	if (!n || !n->has_method("validate_config")) { d["ok"] = false; d["errors"] = args(String("library has no validator: ") + library); return d; }
	Variant r = n->call("validate_config", config);
	if (r.get_type() == Variant::DICTIONARY) return r;
	Array errs = r.get_type() == Variant::PACKED_STRING_ARRAY ? Array(PackedStringArray(r)) : AR(r);
	d["ok"] = errs.is_empty(); d["errors"] = errs;
	return d;
}


Array JVModAPI::events_list() const {
	Array out;
	for (auto &e : A::events()) {
		Dictionary d;
		d["name"] = e.name; d["capability"] = e.cap; d["data"] = e.data; d["when"] = e.when;
		out.push_back(d);
	}
	return out;
}
bool JVModAPI::subscribe(const String &mod, const String &event, const Callable &fn) {
	const A::EventInfo *e = A::find_event(S(event));
	if (!e || !fn.is_valid() || !use(mod, "events.bus") || !use(mod, e->cap, false)) return false;
	for (auto &s : subs_[event]) if (s.mod == mod && s.fn == fn) return true;
	subs_[event].push_back({ mod, fn });
	return true;
}
bool JVModAPI::unsubscribe(const String &mod, const String &event, const Callable &fn) {
	auto it = subs_.find(event);
	if (it == subs_.end()) return false;
	auto &v = it->second;
	for (size_t i = 0; i < v.size(); i++) if (v[i].mod == mod && v[i].fn == fn) { v.erase(v.begin() + i); return true; }
	return false;
}
void JVModAPI::emit_event(const String &name, const Dictionary &data) {
	if (JVDevTools *dv = JVDevTools::get()) if (dv->debug_enabled()) dv->trace("event", name, JSON::stringify(data).left(200));
	emit_signal("event", name, data);
	auto it = subs_.find(name);
	if (it == subs_.end()) return;
	std::vector<Sub> live;
	for (auto &s : it->second) {
		if (!s.fn.is_valid()) continue;
		live.push_back(s);
		s.fn.call(name, data);
	}
	subs_[name] = live;
}

void JVModAPI::hook_goofy_relay() {
	Node *g = lib("goofy");
	uint64_t id = g ? uint64_t(g->get_instance_id()) : 0;
	if (id == goofy_relay_id_) return;
	goofy_relay_id_ = id;
	if (!g) return;
	if (g->has_signal("params_changed")) g->connect("params_changed", Callable(this, "_on_goofy_params_changed"));
	if (g->has_signal("ball_mode_changed")) g->connect("ball_mode_changed", Callable(this, "_on_goofy_ball_mode_changed"));
	if (g->has_signal("physics_allowed_changed")) g->connect("physics_allowed_changed", Callable(this, "_on_goofy_physics_allowed_changed"));
	if (g->has_signal("game_event")) g->connect("game_event", Callable(this, "_on_goofy_game_event"));
	if (g->has_signal("sync_changed")) g->connect("sync_changed", Callable(this, "_on_goofy_sync_changed"));
}
void JVModAPI::_on_goofy_game_event(const String &kind, const String &name, int64_t index, const Array &a) {
	Dictionary d; d["name"] = name; d["index"] = index; d["args"] = a;
	emit_event(kind == "ball" ? "goofy_ball_event" : "goofy_player_event", d);
}
void JVModAPI::_on_goofy_sync_changed(const String &role, int64_t n) {
	Dictionary d; d["role"] = role; d["host_values"] = n; emit_event("goofy_sync_changed", d);
}
void JVModAPI::_on_goofy_params_changed(const Array &changed, const Dictionary &effective) {
	Dictionary d; d["changed"] = changed; d["effective"] = effective; emit_event("goofy_params_changed", d);
}
void JVModAPI::_on_goofy_ball_mode_changed(const String &mode, bool force) {
	Dictionary d; d["mode"] = mode; d["force_defaults"] = force; emit_event("goofy_ball_mode_changed", d);
}
void JVModAPI::_on_goofy_physics_allowed_changed(bool allowed) {
	Dictionary d; d["allowed"] = allowed; emit_event("goofy_physics_allowed_changed", d);
}



void JVModAPI::poll_events() {
	hook_goofy_relay();
	auto changed = [&](const char *ev, String &last, const String &now) {
		if (now == last) return;
		String prev = last; last = now;
		if (!events_primed_) return;
		Dictionary d;
		if (String(ev) == "mode_changed") d["mode"] = now; else d["id"] = now;
		d["previous"] = prev;
		emit_event(ev, d);
	};
	changed("mode_changed", last_mode_, get_mode());
	if (Node *t = lib("textures")) {
		if (t->has_method("get_picked_stage")) changed("stage_picked", last_stage_, STR(t->call("get_picked_stage")));
		if (t->has_method("get_picked_ball")) changed("ball_picked", last_ball_, STR(t->call("get_picked_ball")));
	}
	if (Node *l = lib("loader")) if (l->has_method("get_active_map")) changed("loader_map_changed", last_map_, STR(l->call("get_active_map")));
	if (Node *m = lib("models")) if (m->has_method("get_shown_model")) changed("model_changed", last_model_, STR(m->call("get_shown_model")));
	if (Node *mp = lib("menu_perf")) if (mp->has_method("is_enabled")) {
		int now = B(mp->call("is_enabled")) ? 1 : 0;
		if (now != last_menu_perf_) {
			bool fire = events_primed_ && last_menu_perf_ != -1;
			last_menu_perf_ = now;
			if (fire) { Dictionary d; d["enabled"] = now == 1; emit_event("menu_perf_changed", d); }
		}
	}
	Node *sc = jvm::current_scene(this);
	uint64_t sid = sc ? uint64_t(sc->get_instance_id()) : 0;
	if (sid != last_scene_id_) {
		String prev = last_scene_name_;
		last_scene_id_ = sid;
		last_scene_name_ = sc ? String(sc->get_name()) : String();
		if (events_primed_) {
			Dictionary d; d["scene"] = sc; d["name"] = last_scene_name_; d["previous_name"] = prev;
			emit_event("scene_changed", d);
		}
	}
	events_primed_ = true;
	if (JVDevTools *dv = JVDevTools::get()) dv->consts_tick(0.25);
}


static bool connect_checked(Object *o, const String &sig, const Callable &fn) {
	if (!o || !fn.is_valid() || !o->has_signal(sig)) return false;
	if (o->is_connected(sig, fn)) return true;
	return o->connect(sig, fn) == OK;
}
bool JVModAPI::connect_ball_event(const String &mod, Node *ball, const String &name, const Callable &fn) {
	if (!A::allowed_signal("ball", S(name)) || !jvm::script_is(ball, "Ball")) return false;
	return use(mod, name == "struck" ? "ball.struck" : "ball.events") && connect_checked(ball, name, fn);
}
bool JVModAPI::connect_player_event(const String &mod, Node *player, const String &name, const Callable &fn) {
	if (!A::allowed_signal("player", S(name)) || !jvm::script_is(player, "Player")) return false;
	return use(mod, name == "touched" ? "player.touched" : "player.events") && connect_checked(player, name, fn);
}
bool JVModAPI::connect_net_signal(const String &mod, const String &name, const Callable &fn) {
	if (!A::allowed_signal("net", S(name)) || !use(mod, "net.game_signals") || !is_inside_tree()) return false;
	return connect_checked(get_tree()->get_root()->get_node_or_null(NodePath("Net")), name, fn);
}
static Node *find_named(Node *n, const String &name, int depth) {
	if (!n || depth > 10) return nullptr;
	if (String(n->get_name()) == name) return n;
	for (int i = 0; i < n->get_child_count(); i++) if (Node *f = find_named(n->get_child(i), name, depth + 1)) return f;
	return nullptr;
}

Dictionary JVModAPI::stage_get_nodes(const String &mod) {
	Dictionary d;
	if (!use(mod, "scene.stage")) return d;
	Node *sc = jvm::current_scene(this);
	if (!sc) return d;
	Dictionary spec; spec["script_class"] = "Stage";
	Array st = Array(jvm::find_targets(this, spec));
	d["stage"] = st.is_empty() ? Variant() : st[0];
	Node *court = sc->get_node_or_null(NodePath("Court"));
	if (!court) court = find_named(sc, "Court", 0);
	d["court"] = court;
	d["court_floor"] = court ? court->get_node_or_null(NodePath("CourtFloor")) : nullptr;
	d["net"] = court ? court->get_node_or_null(NodePath("Net")) : nullptr;
	d["grass"] = find_named(sc, "Grass", 0);
	return d;
}


Dictionary JVModAPI::player_get_state(const String &mod, Node *player) {
	Dictionary d;
	if (!use(mod, "player.input_state") || !jvm::script_is(player, "Player")) return d;
	static const char *props[] = { "team_side", "serving", "contact_state", "aim_point", "hold_power" };
	for (const char *p : props) d[p] = player->get(p);
	d["grounded"] = player->has_method("grounded") ? player->call("grounded") : Variant();
	if (Node3D *n3 = Object::cast_to<Node3D>(player)) d["position"] = n3->get_global_position();
	return d;
}


Variant JVModAPI::player_feel_get(const String &mod, const String &prop) {
	if (!use(mod, "player.feel")) return Variant();
	Ref<Resource> r = ResourceLoader::get_singleton()->load("res://game/player/player_feel.tres");
	if (r.is_null()) return Variant();
	return r->get(prop);
}


Dictionary JVModAPI::content_status(const String &mod) { return D(fwd(mod, "net.content_offer", "textures", "get_content_status", args(), Dictionary(), false)); }
bool JVModAPI::content_fetch(const String &mod, const String &id) { return B(fwd(mod, "net.content_download", "textures", "content_fetch", args(id), false)); }
Array JVModAPI::packs_list(const String &mod, const String &type) { return AR(fwd(mod, "packs.query", "textures", "list_picker_packs", args(type), Array())); }
Dictionary JVModAPI::pack_info(const String &mod, const String &id) { return D(fwd(mod, "packs.query", "textures", "pack_info", args(id), Dictionary())); }
bool JVModAPI::packs_rescan(const String &mod) {
	if (!use(mod, "packs.rescan")) return false;
	Node *t = lib("textures");
	if (!t || !t->has_method("rescan_picker_packs")) return false;
	t->call("rescan_picker_packs");
	return true;
}
String JVModAPI::stage_get_picked(const String &mod) { return STR(fwd(mod, "packs.query", "textures", "get_picked_stage", args(), String())); }
String JVModAPI::ball_get_picked(const String &mod) { return STR(fwd(mod, "packs.query", "textures", "get_picked_ball", args(), String())); }
static bool call_ok(Node *n, const char *m, const Array &a) {
	if (!n || !n->has_method(m)) return false;
	Variant r = n->callv(m, a);
	return r.get_type() == Variant::NIL ? true : B(r);
}
bool JVModAPI::stage_set_picked(const String &mod, const String &id) { return use(mod, "stage.pick") && call_ok(lib("textures"), "set_picked_stage", args(id)); }
bool JVModAPI::ball_set_picked(const String &mod, const String &id) { return use(mod, "ball.pick") && call_ok(lib("textures"), "set_picked_ball", args(id)); }
bool JVModAPI::cosmetics_apply(const String &mod) { return use(mod, "stage.pick") && call_ok(lib("textures"), "apply_picked_cosmetics", args()); }
Dictionary JVModAPI::textures_status(const String &mod) { return D(fwd(mod, "packs.query", "textures", "get_status", args(), Dictionary())); }
Dictionary JVModAPI::textures_sync_status(const String &mod) { return D(fwd(mod, "packs.query", "textures", "get_sync_status", args(), Dictionary())); }
Dictionary JVModAPI::textures_cache_info(const String &mod) {
	Dictionary d;
	if (!use(mod, "packs.query")) return d;
	Node *t = lib("textures");
	if (!t) return d;
	if (t->has_method("get_cache_count")) d["count"] = t->call("get_cache_count");
	if (t->has_method("get_disk_load_count")) d["disk_loads"] = t->call("get_disk_load_count");
	if (t->has_method("get_cache_ids")) d["ids"] = t->call("get_cache_ids");
	if (t->has_method("get_swapped_count")) d["swapped"] = t->call("get_swapped_count");
	if (t->has_method("get_override_count")) d["overrides"] = t->call("get_override_count");
	if (t->has_method("get_spawned_count")) d["spawned"] = t->call("get_spawned_count");
	if (t->has_method("get_mip_stats")) d["mips"] = t->call("get_mip_stats");
	return d;
}
Variant JVModAPI::texture_get_cached(const String &mod, const String &path) { return fwd(mod, "packs.query", "textures", "get_cached_texture", args(path)); }
Dictionary JVModAPI::court_env_status(const String &mod) { return D(fwd(mod, "court.query", "textures", "get_env_status", args(), Dictionary())); }
Dictionary JVModAPI::court_sky_status(const String &mod) { return D(fwd(mod, "court.query", "textures", "get_sky_status", args(), Dictionary())); }

Dictionary JVModAPI::court_env_info(const String &mod, const String &stage_id) {
	Dictionary d;
	if (!use(mod, "court.query")) return d;
	Node *t = lib("textures");
	Dictionary info = t && t->has_method("pack_info") ? D(t->call("pack_info", stage_id)) : Dictionary();
	d["found"] = !info.is_empty();
	static const char *keys[] = { "environment", "sky", "court", "base_stage", "label", "name", "has_environment", "has_sky", "has_audio", "audio" };
	for (const char *k : keys) if (info.has(k)) d[k] = info[k];
	if (!d.has("has_environment")) d["has_environment"] = info.has("environment");
	if (!d.has("has_sky")) d["has_sky"] = info.has("sky");
	return d;
}

bool JVModAPI::court_env_load(const String &mod, const String &stage_id) {
	if (!use(mod, "court.env") || !use(mod, "stage.pick")) return false;
	Node *t = lib("textures");
	return call_ok(t, "set_picked_stage", args(stage_id)) && call_ok(t, "apply_picked_cosmetics", args());
}


Array JVModAPI::audio_list(const String &mod) { return AR(fwd(mod, "audio.query", "textures", "audio_list", args(), Array())); }
PackedStringArray JVModAPI::audio_buses(const String &mod) {
	Variant v = fwd(mod, "audio.query", "textures", "audio_buses", args(), PackedStringArray());
	return v.get_type() == Variant::PACKED_STRING_ARRAY ? PackedStringArray(v) : PackedStringArray();
}
Dictionary JVModAPI::audio_info(const String &mod, const String &id) { return D(fwd(mod, "audio.query", "textures", "audio_info", args(id), Dictionary())); }
Dictionary JVModAPI::audio_status(const String &mod) { return D(fwd(mod, "audio.query", "textures", "get_audio_status", args(), Dictionary())); }
int64_t JVModAPI::audio_play(const String &mod, const String &id, const Dictionary &opts) { return int64_t(fwd(mod, "audio.play", "textures", "audio_play", args(id, opts), 0)); }
int64_t JVModAPI::audio_play_loop(const String &mod, const String &id, const Dictionary &opts) { return int64_t(fwd(mod, "audio.play", "textures", "audio_play_loop", args(id, opts), 0)); }
bool JVModAPI::audio_stop(const String &mod, const Variant &handle_or_id) { return B(fwd(mod, "audio.play", "textures", "audio_stop", args(handle_or_id), false)); }
bool JVModAPI::audio_stop_all(const String &mod) { return use(mod, "audio.play") && (fwd(mod, "audio.play", "textures", "audio_stop_all", args()), true); }
bool JVModAPI::audio_set_volume(const String &mod, int64_t handle, double volume) { return B(fwd(mod, "audio.play", "textures", "audio_set_volume", args(handle, volume), false)); }
bool JVModAPI::audio_set_bus(const String &mod, int64_t handle, const String &bus) { return B(fwd(mod, "audio.play", "textures", "audio_set_bus", args(handle, bus), false)); }
int64_t JVModAPI::audio_apply_pack(const String &mod, const String &stage_id) { return int64_t(fwd(mod, "audio.play", "textures", "audio_apply_pack", args(stage_id), 0)); }

Dictionary JVModAPI::accessories_status(const String &mod) { return D(fwd(mod, "accessories.query", "textures", "get_accessory_status", args(), Dictionary())); }
Array JVModAPI::accessories_list(const String &mod, const String &category) {
	Array out;
	Array ents = AR(accessories_status(mod).get("entries", Array()));
	for (int i = 0; i < ents.size(); i++) {
		Dictionary e = ents[i];
		if (category.is_empty() || String(e.get("category", "")) == category) out.push_back(e);
	}
	return out;
}


Array JVModAPI::models_list(const String &mod) { return AR(fwd(mod, "models.query", "models", "get_model_list", args(), Array())); }
String JVModAPI::model_get_saved(const String &mod) { return STR(fwd(mod, "models.query", "models", "get_saved_model", args(), String())); }
String JVModAPI::model_get_shown(const String &mod) { return STR(fwd(mod, "models.query", "models", "get_shown_model", args(), String())); }
bool JVModAPI::model_is_previewing(const String &mod) { return B(fwd(mod, "models.query", "models", "is_previewing", args(), false)); }
Dictionary JVModAPI::model_settings_get(const String &mod, const String &id) { return D(fwd(mod, "models.query", "models", "get_model_settings", args(id), Dictionary())); }
Dictionary JVModAPI::models_status(const String &mod) { return D(fwd(mod, "models.query", "models", "get_status", args(), Dictionary())); }
Variant JVModAPI::models_rig_report(const String &mod) { return fwd(mod, "models.query", "models", "get_rig_report"); }
bool JVModAPI::model_preview(const String &mod, const String &id) { return use(mod, "player.model") && call_ok(lib("models"), "preview_model", args(id)); }
bool JVModAPI::model_apply(const String &mod, const String &id) { return use(mod, "player.model") && call_ok(lib("models"), "apply_model", args(id)); }
bool JVModAPI::model_end_preview(const String &mod) { return use(mod, "player.model", false) && call_ok(lib("models"), "end_preview", args()); }
bool JVModAPI::model_set_color(const String &mod, const String &id, const Color &c) { return use(mod, "player.model") && call_ok(lib("models"), "set_model_color", args(id, c)); }
bool JVModAPI::model_reset_color(const String &mod, const String &id) { return use(mod, "player.model", false) && call_ok(lib("models"), "reset_model_color", args(id)); }
bool JVModAPI::model_set_option(const String &mod, const String &id, const String &key, const Variant &value) { return use(mod, "player.model") && call_ok(lib("models"), "set_model_option", args(id, key, value)); }


Dictionary JVModAPI::loader_status(const String &mod) { return D(fwd(mod, "loader.query", "loader", "get_status", args(), Dictionary())); }
Dictionary JVModAPI::loader_bridge_status(const String &mod) { return D(fwd(mod, "loader.query", "loader", "get_bridge_status", args(), Dictionary())); }
bool JVModAPI::loader_is_map(const String &mod, const String &stage_id) { return B(fwd(mod, "loader.query", "loader", "is_loader_map", args(stage_id), false)); }
String JVModAPI::loader_active_map(const String &mod) { return STR(fwd(mod, "loader.query", "loader", "get_active_map", args(), String())); }
Dictionary JVModAPI::loader_map_goofy_block(const String &mod, const String &stage_id) { return D(fwd(mod, "loader.query", "loader", "get_map_goofy_block", args(stage_id), Dictionary())); }
bool JVModAPI::loader_refresh(const String &mod) { return use(mod, "stage.pick") && call_ok(lib("loader"), "refresh", args()); }





static bool goofy_api(Node *g) { return g && g->has_method("api_version") && g->has_method("set_override"); }
bool JVModAPI::goofy_available(const String &mod) { return use(mod, "goofy.query") && goofy_api(lib("goofy")); }
String JVModAPI::goofy_api_version(const String &mod) { return goofy_available(mod) ? STR(lib("goofy")->call("api_version")) : String(); }
static Dictionary param_dict(const A::GoofyParam &p) {
	Dictionary d;
	d["id"] = p.id; d["section"] = p.section; d["target"] = p.target; d["prop"] = p.prop;
	if (p.is_color) d["default"] = Color(p.v[0], p.v[1], p.v[2], p.v[3]); else d["default"] = p.v[0];
	d["min"] = p.min; d["max"] = p.max; d["unit"] = p.unit; d["apply"] = p.apply; d["is_color"] = p.is_color; d["sync"] = (p.is_color || String(p.section) == "visuals") ? "local" : "host";
	return d;
}
Array JVModAPI::goofy_schema(const String &mod) {
	if (!use(mod, "goofy.query")) return Array();
	Node *g = lib("goofy");
	if (goofy_api(g) && g->has_method("get_schema")) { Array s = AR(g->call("get_schema")); if (!s.is_empty()) return s; }
	Array out;
	for (auto &p : A::goofy_params()) out.push_back(param_dict(p));
	return out;
}
Dictionary JVModAPI::goofy_param_info(const String &mod, const String &id) {
	if (!use(mod, "goofy.query")) return Dictionary();
	const A::GoofyParam *p = A::find_goofy_param(S(id));
	return p ? param_dict(*p) : Dictionary();
}
Variant JVModAPI::goofy_param_get(const String &mod, const String &id) {
	if (!use(mod, "goofy.query")) return Variant();
	Node *g = lib("goofy");
	if (goofy_api(g) && g->has_method("get_param")) return g->call("get_param", id);

	const A::GoofyParam *p = A::find_goofy_param(S(id));
	if (!p) return Variant();
	const char *path = String(p->target) == "PHYS" ? "res://game/ball/ball_physics.tres" : (String(p->target) == "FEEL" ? "res://game/player/player_feel.tres" : nullptr);
	if (!path) return Variant();
	Ref<Resource> r = ResourceLoader::get_singleton()->load(path);
	return r.is_valid() ? r->get(p->prop) : Variant();
}
Dictionary JVModAPI::goofy_params(const String &mod) { return goofy_available(mod) ? D(lib("goofy")->call("get_params")) : Dictionary(); }
Variant JVModAPI::goofy_stock(const String &mod, const String &id) {
	if (goofy_available(mod) && lib("goofy")->has_method("get_stock")) return lib("goofy")->call("get_stock", id);
	Dictionary info = goofy_param_info(mod, id);
	return info.get("default", Variant());
}
Array JVModAPI::goofy_layers(const String &mod) { return goofy_available(mod) && lib("goofy")->has_method("get_layers") ? AR(lib("goofy")->call("get_layers")) : Array(); }
String JVModAPI::goofy_ball_mode_get(const String &mod) { return goofy_available(mod) && lib("goofy")->has_method("get_ball_mode") ? STR(lib("goofy")->call("get_ball_mode")) : String("normal"); }
String JVModAPI::goofy_loader_map(const String &mod) { return goofy_available(mod) && lib("goofy")->has_method("get_loader_map") ? STR(lib("goofy")->call("get_loader_map")) : String(); }
bool JVModAPI::goofy_physics_allowed(const String &mod) { return goofy_available(mod) && lib("goofy")->has_method("is_physics_allowed") && B(lib("goofy")->call("is_physics_allowed")); }
static Dictionary pc_refused(const String &why) { Dictionary d; d["ok"] = false; d["error"] = why; return d; }
Dictionary JVModAPI::player_consts_get(const String &mod) { return D(fwd(mod, "player.consts", "goofy", "player_consts_get", args(), Dictionary(), false)); }
Dictionary JVModAPI::player_consts_set(const String &mod, const String &name, double value) {
	if (!use(mod, "player.consts")) return pc_refused("refused: player.consts not available to " + mod + " here (" + why_not(mod, "player.consts") + ")");
	Node *g = lib("goofy");
	if (!g || !g->has_method("player_consts_set")) return pc_refused("Goofy with player.consts is not installed");
	return D(g->call("player_consts_set", name, value, "mod:" + mod));
}
Dictionary JVModAPI::player_consts_reset(const String &mod, const String &name) {
	if (!use(mod, "player.consts")) return pc_refused("refused: player.consts not available to " + mod + " here (" + why_not(mod, "player.consts") + ")");
	Node *g = lib("goofy");
	if (!g || !g->has_method("player_consts_reset")) return pc_refused("Goofy with player.consts is not installed");
	return D(g->call("player_consts_reset", name, "mod:" + mod));
}
bool JVModAPI::player_consts_locked(const String &mod) { return B(fwd(mod, "player.consts", "goofy", "player_consts_locked", args(), false, false)); }
Dictionary JVModAPI::goofy_status(const String &mod) { return D(fwd(mod, "goofy.query", "goofy", "get_status", args(), Dictionary())); }


static Dictionary split_params(const Dictionary &params, bool want_player, Array &refused) {
	Dictionary keep;
	Array k = params.keys();
	for (int i = 0; i < k.size(); i++) {
		String key = k[i];
		const A::GoofyParam *p = A::find_goofy_param(S(key));
		const bool player = p && A::goofy_is_player_library(*p);
		if (p && player != want_player) {
			refused.push_back(key + (player ? ": player library param - goofy_player_set_override (goofy.player)" : ": ball library param - goofy_set_override (goofy.ball)"));
			continue;
		}
		keep[key] = params[key];
	}
	return keep;
}
static Dictionary goofy_layer_set(JVModAPI *api, Node *g, const String &cap, const String &owner, const Dictionary &params, int64_t priority, const String &scope, bool player) {
	Dictionary r;
	r["ok"] = false; r["applied"] = Array(); r["ignored"] = Array(); r["active"] = false;
	if (!goofy_api(g)) { r["errors"] = args("Goofy API not installed (jvm_goofy without set_override)"); return r; }
	Array refused;
	Dictionary keep = split_params(params, player, refused);
	int64_t pr = priority < 0 ? 0 : (priority > 899 ? 899 : priority);
	String sc = scope == "stage" ? String("stage") : String("session");
	Variant v = g->call("set_override", owner, keep, pr, sc);
	if (v.get_type() == Variant::DICTIONARY) r = v;
	Array er = r.get("errors", Array());
	er.append_array(refused);
	r["errors"] = er;
	return r;
}
Dictionary JVModAPI::goofy_set_override(const String &mod, const Dictionary &params, int64_t priority, const String &scope) {
	if (!use(mod, "goofy.ball")) { Dictionary r; r["ok"] = false; r["applied"] = Array(); r["ignored"] = Array(); r["active"] = false; r["errors"] = args(why_not(mod, "goofy.ball")); return r; }
	return goofy_layer_set(this, lib("goofy"), "goofy.ball", "mod:" + mod, params, priority, scope, false);
}
Dictionary JVModAPI::goofy_player_set_override(const String &mod, const Dictionary &params, int64_t priority, const String &scope) {
	if (!use(mod, "goofy.player")) { Dictionary r; r["ok"] = false; r["applied"] = Array(); r["ignored"] = Array(); r["active"] = false; r["errors"] = args(why_not(mod, "goofy.player")); return r; }
	return goofy_layer_set(this, lib("goofy"), "goofy.player", "mod:" + mod + ":player", params, priority, scope, true);
}
bool JVModAPI::goofy_player_clear_override(const String &mod) {
	if (!use(mod, "goofy.player", false)) return false;
	Node *g = lib("goofy");
	if (!goofy_api(g) || !g->has_method("clear_override")) return false;
	return B(g->call("clear_override", "mod:" + mod + ":player"));
}

Array JVModAPI::goofy_ball_schema(const String &mod) {
	Array s = AR(fwd(mod, "goofy.query", "goofy", "get_ball_schema", args(), Array()));
	if (!s.is_empty() || !use(mod, "goofy.query", false)) return s;
	for (auto &p : A::goofy_params()) if (!A::goofy_is_player_library(p)) { Dictionary d = param_dict(p); d["library"] = "ball"; s.push_back(d); }
	return s;
}
Array JVModAPI::goofy_player_schema(const String &mod) {
	Array s = AR(fwd(mod, "goofy.query", "goofy", "get_player_schema", args(), Array()));
	if (!s.is_empty() || !use(mod, "goofy.query", false)) return s;
	for (auto &p : A::goofy_params()) if (A::goofy_is_player_library(p)) { Dictionary d = param_dict(p); d["library"] = "player"; s.push_back(d); }
	return s;
}
Array JVModAPI::goofy_balls(const String &mod) { return AR(fwd(mod, "goofy.query", "goofy", "list_balls", args(), Array())); }
Dictionary JVModAPI::goofy_ball_state(const String &mod, int64_t index) { return D(fwd(mod, "goofy.query", "goofy", "get_ball_state", args(index), Dictionary())); }
bool JVModAPI::goofy_ball_set_velocity(const String &mod, int64_t index, const Vector3 &v) { return B(fwd(mod, "goofy.ball_live", "goofy", "set_ball_velocity", args(index, v), false)); }
bool JVModAPI::goofy_ball_set_curve(const String &mod, int64_t index, double c) { return B(fwd(mod, "goofy.ball_live", "goofy", "set_ball_curve", args(index, c), false)); }
bool JVModAPI::goofy_ball_impulse(const String &mod, int64_t index, const Vector3 &dv) { return B(fwd(mod, "goofy.ball_live", "goofy", "apply_ball_impulse", args(index, dv), false)); }
Array JVModAPI::goofy_players(const String &mod) { return AR(fwd(mod, "goofy.query", "goofy", "list_players", args(), Array())); }
Dictionary JVModAPI::goofy_player_state(const String &mod, int64_t index) { return D(fwd(mod, "goofy.query", "goofy", "get_player_state", args(index), Dictionary())); }
Dictionary JVModAPI::goofy_player_cosmetics(const String &mod, int64_t index) { return D(fwd(mod, "goofy.query", "goofy", "get_player_cosmetics", args(index), Dictionary())); }
Dictionary JVModAPI::goofy_sync_status(const String &mod) { return D(fwd(mod, "goofy.query", "goofy", "get_sync_status", args(), Dictionary())); }
bool JVModAPI::goofy_clear_override(const String &mod) {
	if (!use(mod, "goofy.ball", false)) return false;
	Node *g = lib("goofy");
	if (!goofy_api(g) || !g->has_method("clear_override")) return false;
	Variant v = g->call("clear_override", "mod:" + mod);
	return v.get_type() == Variant::NIL ? true : B(v);
}


static Node *goofy_bridge_node(Node *g) {
	if (!g || g->is_queued_for_deletion()) return nullptr;
	return g->has_method("api_version") && g->has_method("apply_pack_params") && g->has_method("clear_pack_params") && g->has_method("set_loader_map") ? g : nullptr;
}
Dictionary JVModAPI::goofy_bridge_info(const String &mod) {
	Dictionary d;
	if (!use(mod, "goofy.packs", false)) return d;
	Node *g = goofy_bridge_node(lib("goofy"));
	if (!g) return d;
	d["id"] = int64_t(g->get_instance_id());
	d["version"] = STR(g->call("api_version"));
	d["enabled"] = g->has_method("is_enabled") && B(g->call("is_enabled"));
	return d;
}
bool JVModAPI::goofy_loader_map_set(const String &mod, const String &map) {
	Node *g = use(mod, "goofy.packs", false) ? goofy_bridge_node(lib("goofy")) : nullptr;
	if (g) g->call("set_loader_map", map);
	return g != nullptr;
}
Variant JVModAPI::goofy_pack_apply(const String &mod, const String &map, const Dictionary &params, int64_t version) {
	Node *g = use(mod, "goofy.packs", false) ? goofy_bridge_node(lib("goofy")) : nullptr;
	return g ? g->call("apply_pack_params", map, params, version) : Variant();
}
bool JVModAPI::goofy_pack_clear(const String &mod, const String &map) {
	Node *g = use(mod, "goofy.packs", false) ? goofy_bridge_node(lib("goofy")) : nullptr;
	if (g) g->call("clear_pack_params", map);
	return g != nullptr;
}
bool JVModAPI::goofy_ball_mode_set(const String &mod, const String &mode) {
	if (mode != "normal" && mode != "fixed") return false;
	return use(mod, "goofy.ball_mode") && goofy_api(lib("goofy")) && call_ok(lib("goofy"), "set_ball_mode", args(mode));
}
Dictionary JVModAPI::goofy_rules_get(const String &mod) {
	Dictionary d;
	if (!use(mod, "goofy.query")) return d;
	Node *g = lib("goofy");
	if (!g) return d;
	if (g->has_method("get_max_hits")) d["max_hits"] = g->call("get_max_hits");
	if (g->has_method("get_no_double_hit")) d["no_double_hit"] = g->call("get_no_double_hit");
	return d;
}
bool JVModAPI::goofy_rules_set(const String &mod, int64_t max_hits, bool no_double_hit) {
	if (!use(mod, "goofy.rules")) return false;
	Node *g = lib("goofy");
	if (!g || !g->has_method("set_max_hits") || !g->has_method("set_no_double_hit")) return false;
	g->call("set_max_hits", max_hits);
	g->call("set_no_double_hit", no_double_hit);
	return true;
}


PackedStringArray JVModAPI::shots_list(const String &mod) {
	Variant v = fwd(mod, "shots.query", "shots", "get_shot_ids");
	return v.get_type() == Variant::PACKED_STRING_ARRAY ? PackedStringArray(v) : PackedStringArray(AR(v));
}
String JVModAPI::shots_armed(const String &mod) { return STR(fwd(mod, "shots.query", "shots", "get_armed", args(), String())); }

Variant JVModAPI::shots_last(const String &mod, const String &kind) {
	static const char *k[][2] = { { "spin", "get_last_spin" }, { "topspin", "get_last_topspin" }, { "curve", "get_last_curve" }, { "float", "get_last_float" }, { "event", "get_last_event" } };
	for (auto &e : k) if (kind == e[0]) return fwd(mod, "shots.query", "shots", e[1]);
	return Variant();
}
Variant JVModAPI::shots_spin_bar(const String &mod) { return fwd(mod, "shots.query", "shots", "get_spin_bar"); }
Variant JVModAPI::shots_range_box(const String &mod) { return fwd(mod, "shots.query", "shots", "get_range_box"); }
Dictionary JVModAPI::shots_counts(const String &mod) {
	Dictionary d;
	if (!use(mod, "shots.query")) return d;
	Node *s = lib("shots");
	if (!s) return d;
	if (s->has_method("get_applied_count")) d["applied"] = s->call("get_applied_count");
	if (s->has_method("get_skipped_count")) d["skipped"] = s->call("get_skipped_count");
	if (s->has_method("get_touch_count")) d["touches"] = s->call("get_touch_count");
	return d;
}
Dictionary JVModAPI::shots_status(const String &mod) { return D(fwd(mod, "shots.query", "shots", "get_status", args(), Dictionary())); }
bool JVModAPI::shot_arm(const String &mod, const String &id) { return use(mod, "shots.arm") && call_ok(lib("shots"), "arm", args(id)); }
bool JVModAPI::shots_debug_set(const String &mod, bool on) { return use(mod, "shots.debug", !on) && call_ok(lib("shots"), "set_debug_visible", args(on)); }
bool JVModAPI::shots_debug_visible(const String &mod) { return B(fwd(mod, "shots.debug", "shots", "is_debug_visible", args(), false)); }
String JVModAPI::shots_debug_text(const String &mod) { return STR(fwd(mod, "shots.debug", "shots", "get_debug_text", args(), String())); }


bool JVModAPI::picker_ball_open(const String &mod) { return use(mod, "stage.picker") && call_ok(lib("picker"), "ball_picker_open", args()); }
bool JVModAPI::picker_stage_inject(const String &mod) { return use(mod, "stage.picker") && call_ok(lib("picker"), "inject_stage_tiles", args()); }
bool JVModAPI::mods_panel_open(const String &mod) { return use(mod, "ui.mods_panel") && call_ok(lib("menu_ui"), "open_mods_panel", args()); }
bool JVModAPI::mods_panel_close(const String &mod) { return use(mod, "ui.mods_panel", false) && call_ok(lib("menu_ui"), "close_mods_panel", args()); }
bool JVModAPI::mods_panel_show_list(const String &mod) { return use(mod, "ui.mods_panel") && call_ok(lib("menu_ui"), "show_list", args()); }
bool JVModAPI::mods_panel_show_detail(const String &mod, const String &mod_id) { return use(mod, "ui.mods_panel") && call_ok(lib("menu_ui"), "show_detail", args(mod_id)); }
Dictionary JVModAPI::mods_panel_info(const String &mod) { return D(fwd(mod, "ui.mods_panel", "menu_ui", "get_info", args(), Dictionary())); }
Variant JVModAPI::mods_config_files(const String &mod, const String &mod_id) { return fwd(mod, "ui.mods_panel", "menu_ui", "config_files", args(mod_id)); }
Variant JVModAPI::mods_config_save(const String &mod, const String &path, const String &text, bool live) { return fwd(mod, "ui.mods_panel", "menu_ui", "validate_and_save", args(path, text, live)); }
bool JVModAPI::menu_perf_get(const String &mod) { return B(fwd(mod, "menu.perf", "menu_perf", "is_enabled", args(), false)); }
bool JVModAPI::menu_perf_set(const String &mod, bool on) { return use(mod, "menu.perf") && call_ok(lib("menu_perf"), "set_enabled", args(on)); }
Dictionary JVModAPI::menu_perf_info(const String &mod) { return D(fwd(mod, "menu.perf", "menu_perf", "get_info", args(), Dictionary())); }
bool JVModAPI::menu_perf_reset(const String &mod) { return use(mod, "menu.perf") && call_ok(lib("menu_perf"), "reset_counters", args()); }





void JVModAPI::load_script_mods() {
	JVModGate *g = gate();
	if (!g || !g->pin_ok() || !is_inside_tree()) return;
	String dir = jvm::mods_root().path_join("api_mods");
	if (!DirAccess::dir_exists_absolute(dir)) return;
	PackedStringArray names = DirAccess::get_directories_at(dir);
	names.sort();
	Node *root = get_tree()->get_root();
	Node *host = root->get_node_or_null(NodePath("JVScriptMods"));
	for (int i = 0; i < names.size(); i++) {
		String base = dir.path_join(names[i]);
		String gd = base.path_join("mod.gd");
		if (!FileAccess::file_exists(gd)) continue;
		Dictionary meta = FileAccess::file_exists(base.path_join("mod.json")) ? D(jvm::load_json(base.path_join("mod.json"))) : Dictionary();
		if (meta.has("enabled") && !bool(meta["enabled"])) { jvm::log("api", "script mod " + names[i] + " disabled (mod.json)"); continue; }
		Ref<Script> sc = ResourceLoader::get_singleton()->load(gd, "", ResourceLoader::CACHE_MODE_IGNORE);
		if (sc.is_null() || !sc->can_instantiate()) { jvm::warn("api", "script mod " + names[i] + ": cannot load " + gd); continue; }
		if (!ClassDB::is_parent_class(sc->get_instance_base_type(), "Node")) { jvm::warn("api", "script mod " + names[i] + ": mod.gd must extend Node"); continue; }
		Object *o = ClassDB::instantiate(sc->get_instance_base_type());
		Node *n = Object::cast_to<Node>(o);
		if (!n) { if (o) memdelete(o); continue; }
		n->set_script(sc);
		n->set_name(names[i].validate_node_name());
		n->set_meta("jvm_path", gd);
		if (!host) { host = memnew(Node); host->set_name("JVScriptMods"); root->add_child(host); }
		host->add_child(n);
		jvm::log("api", "script mod loaded: " + names[i]);
	}
}

Array JVModAPI::script_mods_list() const {
	Array out;
	if (!is_inside_tree()) return out;
	Node *host = get_tree()->get_root()->get_node_or_null(NodePath("JVScriptMods"));
	if (!host) return out;
	for (int i = 0; i < host->get_child_count(); i++) {
		Node *c = host->get_child(i);
		Dictionary d;
		d["name"] = String(c->get_name());
		d["path"] = c->has_meta("jvm_path") ? c->get_meta("jvm_path") : Variant("");
		out.push_back(d);
	}
	return out;
}







Variant JVModAPI::io_get(const String &mod, const String &cap, Object *o, const StringName &prop) {
	if (!o) return Variant();
	CharString c = cap.utf8();
	if (!use(mod, c.get_data(), false)) return Variant();
	return o->get(prop);
}
bool JVModAPI::io_set(const String &mod, const String &cap, Object *o, const StringName &prop, const Variant &v, bool undo) {
	if (!o) return false;
	CharString c = cap.utf8();
	if (!use(mod, c.get_data(), !undo)) return false;
	o->set(prop, v);
	return true;
}
Variant JVModAPI::io_call(const String &mod, const String &cap, Object *o, const StringName &method, const Array &args, bool undo) {
	if (!o) return Variant();
	CharString c = cap.utf8();
	if (!use(mod, c.get_data(), !undo)) return Variant();
	Ref<Script> sc = o->get_script();
	const String key = String::num_uint64(sc.is_valid() ? uint64_t(sc->get_instance_id()) : 0) + String("|") + String(method);
	auto it = rpc_cache_.find(key);
	bool rpc = it != rpc_cache_.end() ? it->second : (rpc_cache_[key] = is_rpc(o, method));
	if (rpc) { jvm::warn("api", "refused " + mod + " -> " + String(method) + ": game RPCs are never called by mods"); return Variant(); }
	return o->callv(method, args);
}
int64_t JVModAPI::io_connect(const String &mod, const String &cap, Object *o, const StringName &sig, const Callable &cb, int64_t flags) {
	if (!o) return ERR_INVALID_PARAMETER;
	CharString c = cap.utf8();
	if (!use(mod, c.get_data(), false)) return ERR_UNAUTHORIZED;
	return o->connect(sig, cb, uint32_t(flags));
}
bool JVModAPI::io_disconnect(const String &mod, const String &cap, Object *o, const StringName &sig, const Callable &cb) {
	if (!o || !o->is_connected(sig, cb)) return false;
	CharString c = cap.utf8();
	if (!use(mod, c.get_data(), false)) return false;
	o->disconnect(sig, cb);
	return true;
}
Node *JVModAPI::io_node(const String &mod, const String &cap, Node *from, const String &path) {
	if (!from) return nullptr;
	CharString c = cap.utf8();
	if (!use(mod, c.get_data(), false)) return nullptr;
	return from->get_node_or_null(NodePath(path));
}
Node *JVModAPI::io_autoload(const String &mod, const String &cap, const String &name) {
	if (!is_inside_tree()) return nullptr;
	CharString c = cap.utf8();
	if (!use(mod, c.get_data(), false)) return nullptr;
	return get_tree()->get_root()->get_node_or_null(NodePath(name));
}
Ref<Resource> JVModAPI::io_resource(const String &mod, const String &cap, const String &path) {
	CharString c = cap.utf8();
	if (!path.begins_with("res://") || !use(mod, c.get_data(), false)) return Ref<Resource>();
	ResourceLoader *rl = ResourceLoader::get_singleton();
	return rl->exists(path) ? rl->load(path) : Ref<Resource>();
}
Object *JVModAPI::io_singleton(const String &mod, const String &cap, const String &name) {
	Engine *e = Engine::get_singleton();
	if (!e->has_singleton(name)) return nullptr;
	CharString c = cap.utf8();
	if (!use(mod, c.get_data(), false)) return nullptr;
	return e->get_singleton(name);
}


void JVModAPI::bind_core_methods() {
#define BM(name, ...) ClassDB::bind_method(D_METHOD(#name, ##__VA_ARGS__), &JVModAPI::name)
	BM(gate_matrix); BM(net_safety, "cap");
	BM(libraries_status, "mod"); BM(library_status, "mod", "library"); BM(library_reload, "mod", "library");
	BM(state_get, "mod", "key", "default"); BM(state_set, "mod", "key", "value"); BM(state_erase, "mod", "key"); BM(state_keys, "mod");
	BM(config_validate, "mod", "library", "config");
	BM(events_list); BM(subscribe, "mod", "event", "callable"); BM(unsubscribe, "mod", "event", "callable");
	BM(_on_goofy_params_changed, "changed", "effective"); BM(_on_goofy_ball_mode_changed, "mode", "force_defaults"); BM(_on_goofy_physics_allowed_changed, "allowed");
	BM(connect_ball_event, "mod", "ball", "name", "callable"); BM(connect_player_event, "mod", "player", "name", "callable"); BM(connect_net_signal, "mod", "name", "callable");
	BM(stage_get_nodes, "mod"); BM(player_get_state, "mod", "player"); BM(player_feel_get, "mod", "prop");
	BM(content_status, "mod"); BM(content_fetch, "mod", "id");
	BM(packs_list, "mod", "type"); BM(pack_info, "mod", "id"); BM(packs_rescan, "mod");
	BM(stage_get_picked, "mod"); BM(stage_set_picked, "mod", "id"); BM(ball_get_picked, "mod"); BM(ball_set_picked, "mod", "id"); BM(cosmetics_apply, "mod");
	BM(textures_status, "mod"); BM(textures_sync_status, "mod"); BM(textures_cache_info, "mod"); BM(texture_get_cached, "mod", "path");
	BM(court_env_status, "mod"); BM(court_sky_status, "mod"); BM(court_env_info, "mod", "stage_id"); BM(court_env_load, "mod", "stage_id");
	BM(audio_list, "mod"); BM(audio_buses, "mod"); BM(audio_info, "mod", "id"); BM(audio_status, "mod");
	ClassDB::bind_method(D_METHOD("audio_play", "mod", "id", "opts"), &JVModAPI::audio_play, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("audio_play_loop", "mod", "id", "opts"), &JVModAPI::audio_play_loop, DEFVAL(Dictionary()));
	BM(audio_stop, "mod", "handle_or_id"); BM(audio_stop_all, "mod");
	BM(audio_set_volume, "mod", "handle", "volume"); BM(audio_set_bus, "mod", "handle", "bus");
	BM(audio_apply_pack, "mod", "stage_id");
	BM(accessories_status, "mod"); BM(accessories_list, "mod", "category");
	BM(models_list, "mod"); BM(model_get_saved, "mod"); BM(model_get_shown, "mod"); BM(model_is_previewing, "mod"); BM(model_settings_get, "mod", "id");
	BM(models_status, "mod"); BM(models_rig_report, "mod"); BM(model_preview, "mod", "id"); BM(model_apply, "mod", "id"); BM(model_end_preview, "mod");
	BM(model_set_color, "mod", "id", "color"); BM(model_reset_color, "mod", "id"); BM(model_set_option, "mod", "id", "key", "value");
	BM(loader_status, "mod"); BM(loader_bridge_status, "mod"); BM(loader_is_map, "mod", "stage_id"); BM(loader_active_map, "mod"); BM(loader_map_goofy_block, "mod", "stage_id"); BM(loader_refresh, "mod");
	BM(goofy_available, "mod"); BM(goofy_api_version, "mod"); BM(goofy_schema, "mod"); BM(goofy_param_info, "mod", "id"); BM(goofy_param_get, "mod", "id");
	BM(goofy_params, "mod"); BM(goofy_stock, "mod", "id"); BM(goofy_layers, "mod"); BM(goofy_ball_mode_get, "mod"); BM(goofy_loader_map, "mod");
	BM(goofy_physics_allowed, "mod"); BM(goofy_status, "mod");
	BM(player_consts_get, "mod"); BM(player_consts_set, "mod", "name", "value"); BM(player_consts_reset, "mod", "name"); BM(player_consts_locked, "mod"); BM(goofy_clear_override, "mod"); BM(goofy_ball_mode_set, "mod", "mode");
	ClassDB::bind_method(D_METHOD("goofy_set_override", "mod", "params", "priority", "scope"), &JVModAPI::goofy_set_override, DEFVAL(500), DEFVAL("session"));
	BM(goofy_rules_get, "mod"); BM(goofy_rules_set, "mod", "max_hits", "no_double_hit");
	BM(goofy_ball_schema, "mod"); BM(goofy_player_schema, "mod"); BM(goofy_balls, "mod"); BM(goofy_ball_state, "mod", "index");
	BM(goofy_ball_set_velocity, "mod", "index", "velocity"); BM(goofy_ball_set_curve, "mod", "index", "curve"); BM(goofy_ball_impulse, "mod", "index", "delta_v");
	BM(goofy_players, "mod"); BM(goofy_player_state, "mod", "index"); BM(goofy_player_cosmetics, "mod", "index");
	ClassDB::bind_method(D_METHOD("goofy_player_set_override", "mod", "params", "priority", "scope"), &JVModAPI::goofy_player_set_override, DEFVAL(500), DEFVAL("session"));
	BM(goofy_player_clear_override, "mod"); BM(goofy_sync_status, "mod");
	BM(_on_goofy_game_event, "kind", "name", "index", "args"); BM(_on_goofy_sync_changed, "role", "host_values");
	BM(shots_list, "mod"); BM(shots_armed, "mod"); BM(shots_last, "mod", "kind"); BM(shots_spin_bar, "mod"); BM(shots_range_box, "mod");
	BM(shots_counts, "mod"); BM(shots_status, "mod"); BM(shot_arm, "mod", "id"); BM(shots_debug_set, "mod", "on"); BM(shots_debug_visible, "mod"); BM(shots_debug_text, "mod");
	BM(picker_ball_open, "mod"); BM(picker_stage_inject, "mod");
	BM(mods_panel_open, "mod"); BM(mods_panel_close, "mod"); BM(mods_panel_show_list, "mod"); BM(mods_panel_show_detail, "mod", "mod_id"); BM(mods_panel_info, "mod");
	BM(mods_config_files, "mod", "mod_id"); BM(mods_config_save, "mod", "path", "text", "live");
	BM(menu_perf_get, "mod"); BM(menu_perf_set, "mod", "on"); BM(menu_perf_info, "mod"); BM(menu_perf_reset, "mod");
	BM(script_mods_list); BM(load_script_mods);
	BM(io_get, "mod", "cap", "object", "prop"); BM(io_set, "mod", "cap", "object", "prop", "value", "undo");
	BM(io_call, "mod", "cap", "object", "method", "args", "undo"); BM(io_connect, "mod", "cap", "object", "signal", "callable", "flags");
	BM(io_disconnect, "mod", "cap", "object", "signal", "callable"); BM(io_node, "mod", "cap", "from", "path");
	BM(io_autoload, "mod", "cap", "name"); BM(io_singleton, "mod", "cap", "name"); BM(io_resource, "mod", "cap", "path");
	BM(goofy_bridge_info, "mod"); BM(goofy_loader_map_set, "mod", "map"); BM(goofy_pack_apply, "mod", "map", "params", "version"); BM(goofy_pack_clear, "mod", "map");
#undef BM
	ADD_SIGNAL(MethodInfo("event", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::DICTIONARY, "data")));
}
