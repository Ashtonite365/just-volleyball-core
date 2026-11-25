#include "mod_api.hpp"
#include "../common/jv_prof.hpp"
#include "mod_gate.hpp"
#include "devtools.hpp"

#include <algorithm>

#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;
namespace A = jvm::api;

static const char *kAccess[] = { "read", "local_ui", "cosmetic", "gameplay", "sync", "offline" };
static bool access_from(const String &s, A::Access &out) {
	static const A::Access v[] = { A::Access::Read, A::Access::LocalUi, A::Access::Cosmetic, A::Access::Gameplay, A::Access::Sync, A::Access::Offline };
	for (int i = 0; i < 6; i++) if (s == kAccess[i]) { out = v[i]; return true; }
	return false;
}
static std::string S(const String &s) { return std::string(s.utf8().get_data()); }
static String G(const std::string &s) { return String::utf8(s.c_str()); }

void JVModAPI::_bind_methods() {
	bind_dev_methods();
	bind_gameplay_settings_methods();
	ClassDB::bind_method(D_METHOD("_on_gate_changed", "active", "reason"), &JVModAPI::_on_gate_changed);
	ClassDB::bind_method(D_METHOD("get_api_version"), &JVModAPI::get_api_version);
	ClassDB::bind_method(D_METHOD("get_capabilities"), &JVModAPI::get_capabilities);
	ClassDB::bind_method(D_METHOD("has_capability", "id"), &JVModAPI::has_capability);
	ClassDB::bind_method(D_METHOD("can", "mod", "cap"), &JVModAPI::can);
	ClassDB::bind_method(D_METHOD("why_not", "mod", "cap"), &JVModAPI::why_not);
	ClassDB::bind_method(D_METHOD("is_open", "access"), &JVModAPI::is_open);
	ClassDB::bind_method(D_METHOD("get_reason", "access"), &JVModAPI::get_reason);
	ClassDB::bind_method(D_METHOD("get_mode"), &JVModAPI::get_mode);
	ClassDB::bind_method(D_METHOD("get_status"), &JVModAPI::get_status);
	ClassDB::bind_method(D_METHOD("get_coverage"), &JVModAPI::get_coverage);
	ClassDB::bind_method(D_METHOD("register_mod", "name", "version", "caps"), &JVModAPI::register_mod);
	ClassDB::bind_method(D_METHOD("get_mods"), &JVModAPI::get_mods);
	ClassDB::bind_method(D_METHOD("library_allowed", "short_name"), &JVModAPI::library_allowed);
	ClassDB::bind_method(D_METHOD("is_strict"), &JVModAPI::is_strict);
	ClassDB::bind_method(D_METHOD("load_config", "mod", "category"), &JVModAPI::load_config);
	ClassDB::bind_method(D_METHOD("get_current_scene", "mod"), &JVModAPI::get_current_scene);
	ClassDB::bind_method(D_METHOD("find_balls", "mod"), &JVModAPI::find_balls);
	ClassDB::bind_method(D_METHOD("find_players", "mod"), &JVModAPI::find_players);
	ClassDB::bind_method(D_METHOD("is_ball", "mod", "o"), &JVModAPI::is_ball);
	ClassDB::bind_method(D_METHOD("get_local_player", "mod"), &JVModAPI::get_local_player);
	ClassDB::bind_method(D_METHOD("get_rally_for", "mod", "player"), &JVModAPI::get_rally_for);
	ClassDB::bind_method(D_METHOD("get_net_state", "mod"), &JVModAPI::get_net_state);
	ClassDB::bind_method(D_METHOD("lobby_data_get", "mod", "key"), &JVModAPI::lobby_data_get);
	ClassDB::bind_method(D_METHOD("lobby_data_set", "mod", "key", "value"), &JVModAPI::lobby_data_set);
	ClassDB::bind_method(D_METHOD("lobby_data_clear", "mod", "key"), &JVModAPI::lobby_data_clear);
	ClassDB::bind_method(D_METHOD("ball_physics_open", "mod", "path"), &JVModAPI::ball_physics_open);
	ClassDB::bind_method(D_METHOD("ball_physics_close", "mod"), &JVModAPI::ball_physics_close);
	ClassDB::bind_method(D_METHOD("ball_physics_get", "mod", "prop"), &JVModAPI::ball_physics_get);
	ClassDB::bind_method(D_METHOD("ball_physics_set", "mod", "prop", "value"), &JVModAPI::ball_physics_set);
	ClassDB::bind_method(D_METHOD("ball_physics_restore", "mod", "prop", "value"), &JVModAPI::ball_physics_restore);
	ClassDB::bind_method(D_METHOD("ball_get_model", "mod", "ball"), &JVModAPI::ball_get_model);
	ClassDB::bind_method(D_METHOD("ball_get_flight", "mod", "ball"), &JVModAPI::ball_get_flight);
	ClassDB::bind_method(D_METHOD("ball_set_flight", "mod", "ball", "flight"), &JVModAPI::ball_set_flight);
	ClassDB::bind_method(D_METHOD("connect_ball_struck", "mod", "ball", "callable", "flags"), &JVModAPI::connect_ball_struck, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("node_get_scale", "mod", "cap", "node"), &JVModAPI::node_get_scale);
	ClassDB::bind_method(D_METHOD("node_set_scale", "mod", "cap", "node", "scale"), &JVModAPI::node_set_scale);
	ClassDB::bind_method(D_METHOD("node_restore_scale", "mod", "cap", "node", "scale"), &JVModAPI::node_restore_scale);
	ClassDB::bind_method(D_METHOD("connect_player_touched", "mod", "player", "callable"), &JVModAPI::connect_player_touched);
	ClassDB::bind_method(D_METHOD("rally_touch_state", "mod", "rally"), &JVModAPI::rally_touch_state);
	ClassDB::bind_method(D_METHOD("rally_set_touch_count", "mod", "rally", "count"), &JVModAPI::rally_set_touch_count);
	ClassDB::bind_method(D_METHOD("rally_clear_last_toucher", "mod", "rally"), &JVModAPI::rally_clear_last_toucher);
	ClassDB::bind_method(D_METHOD("modifiers_panel_find", "mod"), &JVModAPI::modifiers_panel_find);
	ClassDB::bind_method(D_METHOD("modifiers_panel_rows", "mod", "panel"), &JVModAPI::modifiers_panel_rows);
	ClassDB::bind_method(D_METHOD("modifiers_panel_add_rows", "mod", "panel", "defs", "on_changed"), &JVModAPI::modifiers_panel_add_rows);
	ClassDB::bind_method(D_METHOD("modifiers_panel_show_rows", "mod", "rows", "show"), &JVModAPI::modifiers_panel_show_rows);
	ClassDB::bind_method(D_METHOD("modifiers_panel_set_values", "mod", "panel", "values"), &JVModAPI::modifiers_panel_set_values);
	ClassDB::bind_method(D_METHOD("time_scale_get"), &JVModAPI::time_scale_get);
	ClassDB::bind_method(D_METHOD("time_scale_set", "mod", "value"), &JVModAPI::time_scale_set);
	ClassDB::bind_method(D_METHOD("time_scale_restore", "mod", "value"), &JVModAPI::time_scale_restore);
	ClassDB::bind_method(D_METHOD("overrides_set_rules", "mod", "rules"), &JVModAPI::overrides_set_rules);
	ClassDB::bind_method(D_METHOD("overrides_apply", "mod"), &JVModAPI::overrides_apply);
	ClassDB::bind_method(D_METHOD("overrides_revert", "mod"), &JVModAPI::overrides_revert);
	ClassDB::bind_method(D_METHOD("game_get", "mod", "object", "prop"), &JVModAPI::game_get);
	ClassDB::bind_method(D_METHOD("game_set", "mod", "object", "prop", "value"), &JVModAPI::game_set);
	ClassDB::bind_method(D_METHOD("game_call", "mod", "object", "method", "args"), &JVModAPI::game_call, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("game_connect", "mod", "object", "signal", "callable"), &JVModAPI::game_connect);
	bind_core_methods();
	ADD_SIGNAL(MethodInfo("access_changed", PropertyInfo(Variant::STRING, "access"), PropertyInfo(Variant::BOOL, "open"), PropertyInfo(Variant::STRING, "reason")));
}

JVModAPI::JVModAPI() {
	set_process_priority(-999);
}

JVModGate *JVModAPI::gate() const { return Object::cast_to<JVModGate>(get_parent()); }




const A::GateView &JVModAPI::view() const {
	Engine *eng = Engine::get_singleton();
	const uint64_t pf = eng->get_process_frames(), ph = eng->get_physics_frames();
	if (!view_dirty_ && pf == view_pf_ && ph == view_ph_) return view_cache_;
	view_cache_ = view_uncached();
	view_pf_ = pf; view_ph_ = ph; view_dirty_ = false;
	return view_cache_;
}
A::GateView JVModAPI::view_uncached() const {
	A::GateView g;
	JVModGate *gt = gate();
	if (closing_) { g.pin_ok = gt && gt->pin_ok(); g.mode = "unknown"; g.full_reason = g.cosmetic_reason = "mod API shutting down (core removed) - fail closed"; return g; }
	if (!gt) { g.mode = "unknown"; g.full_reason = g.cosmetic_reason = "gate not found (core not loaded) - fail closed"; return g; }
	g.pin_ok = gt->pin_ok();
	g.full = gt->is_active();
	g.cosmetic = gt->is_cosmetic_active();
	g.mode = S(gt->get_mode());
	g.full_reason = S(gt->get_reason());
	g.cosmetic_reason = S(gt->get_cosmetic_reason());
	return g;
}

void JVModAPI::_ready() {
	if (JVModGate *gt = gate()) {
		gt->connect("gate_changed", Callable(this, "_on_gate_changed"));
		gt->connect("cosmetic_gate_changed", Callable(this, "_on_gate_changed"));
	}
	Engine *e = Engine::get_singleton();
	if (!e->has_singleton("JVModAPI")) e->register_singleton("JVModAPI", this);
	emit_changes();
	gs_attach_ui();
	call_deferred("load_script_mods");
	jvm::log("api", "mod API " + get_api_version() + " ready: " + String::num_int64(A::capabilities().size()) + " capabilities");
}



void JVModAPI::_exit_tree() {
	if (is_inside_tree() && get_tree()->is_connected("node_added", callable_mp(this, &JVModAPI::_on_node_added_ball)))
		get_tree()->disconnect("node_added", callable_mp(this, &JVModAPI::_on_node_added_ball));
	closing_ = true;
	view_dirty_ = true;
	emit_changes();
	Engine *e = Engine::get_singleton();
	if (e->has_singleton("JVModAPI") && e->get_singleton("JVModAPI") == this) e->unregister_singleton("JVModAPI");
}

void JVModAPI::_process(double delta) {
	JVM_PROF("core.api_process");
	emit_changes();
	gs_tick(delta);
	poll_t_ += delta;
	if (poll_t_ >= 0.25) { JVM_PROF("core.api_process.poll_events"); poll_t_ = 0.0; poll_events(); }
}




void JVModAPI::emit_changes() {
	A::GateView g = view();
	for (int i = 0; i < 6; i++) {
		A::Access a; access_from(kAccess[i], a);
		std::string why;
		bool open = A::access_open(a, g, &why);
		auto it = last_open_.find(kAccess[i]);
		if (it != last_open_.end() && it->second == open) continue;
		last_open_[kAccess[i]] = open;
		const String reason = open ? String("open (" + G(g.mode) + ")") : G(why);
		emit_signal("access_changed", String(kAccess[i]), open, reason);
		Dictionary ev; ev["access"] = String(kAccess[i]); ev["open"] = open; ev["reason"] = reason;
		emit_event("access_changed", ev);
	}
}

bool JVModAPI::use(const String &mod, const char *cap, bool need_open) {
	A::CheckResult r = reg_.check(S(mod), cap, view(), need_open);
	if (r.ok && need_open && gs_match_veto(mod, cap)) {
		r.ok = false;
		r.why = "off for this match: a player in the lobby does not have " + S(mod) + " (docs/GAMEPLAY-SETTINGS.md)";
	}
	if (!r.ok) {
		String key = mod + String("|") + String(cap) + String("|") + G(r.why);
		if (!logged_refusals_.count(key) && logged_refusals_.size() < 256) {
			logged_refusals_.insert(key);
			jvm::log("api", "refused " + mod + " -> " + String(cap) + ": " + G(r.why));
		}
		if (JVDevTools *d = JVDevTools::get()) d->trace("refused", mod + String(" -> ") + String(cap), G(r.why));
	}
	return r.ok;
}

void JVModAPI::load_strict() {
	if (strict_loaded_) return;
	strict_loaded_ = true;
	String p = jvm::mods_root().path_join("api.json");
	if (!FileAccess::file_exists(p)) return;
	Variant v = jvm::load_json(p);
	if (v.get_type() == Variant::DICTIONARY) strict_ = bool(Dictionary(v).get("strict", false));
	jvm::log("api", String("strict mode ") + (strict_ ? "ON (only migrated libraries load)" : "off"));
}


String JVModAPI::get_api_version() const { return G(A::version_string()); }

Array JVModAPI::get_capabilities() const {
	Array out;
	A::GateView g = view();
	for (auto &c : A::capabilities()) {
		Dictionary d;
		d["id"] = c.id; d["since"] = c.since; d["access"] = A::to_string(c.access); d["scope"] = A::to_string(c.scope);
		d["status"] = A::to_string(c.status); d["feature"] = String::utf8(c.feature); d["game_refs"] = c.game_refs; d["functions"] = c.functions;
		d["open"] = c.status != A::Status::Planned && A::access_open(c.access, g);
		out.push_back(d);
	}
	return out;
}

bool JVModAPI::has_capability(const String &id) const {
	const A::Capability *c = A::find(S(id));
	return c && c->status != A::Status::Planned;
}
bool JVModAPI::can(const String &mod, const String &cap) const { return reg_.peek(S(mod), S(cap), view()).ok && !gs_match_veto(mod, S(cap).c_str()); }
String JVModAPI::why_not(const String &mod, const String &cap) const {
	A::CheckResult r = reg_.peek(S(mod), S(cap), view());
	if (r.ok && gs_match_veto(mod, S(cap).c_str())) return "off for this match: a player in the lobby does not have " + mod + " (docs/GAMEPLAY-SETTINGS.md)";
	return r.ok ? String() : G(r.why);
}
bool JVModAPI::is_open(const String &access) const {
	A::Access a;
	return access_from(access, a) && A::access_open(a, view());
}


String JVModAPI::get_reason(const String &access) const {
	A::Access a;
	if (!access_from(access, a)) return "unknown access class " + access;
	A::GateView g = view();
	std::string why;
	if (!A::access_open(a, g, &why) && !why.empty() && (a == A::Access::Sync || a == A::Access::Offline || !g.pin_ok)) return G(why);
	if (a == A::Access::Cosmetic) return G(g.cosmetic_reason);
	if (a == A::Access::Read || a == A::Access::LocalUi) return g.pin_ok ? String("open") : G(why);
	return G(g.full_reason);
}
String JVModAPI::get_mode() const { return G(view().mode); }

Dictionary JVModAPI::get_coverage() const {
	A::Coverage c = A::coverage(&reg_);
	Dictionary d;
	d["capabilities"] = c.total; d["impl"] = c.impl; d["mapped"] = c.mapped; d["planned"] = c.planned;
	d["game_refs"] = c.game_refs; d["used_by_mods"] = c.used_by_mods;
	Dictionary ba;
	for (auto &kv : c.by_access) ba[G(kv.first)] = kv.second;
	d["by_access"] = ba;
	return d;
}

Dictionary JVModAPI::get_mods() const {
	Dictionary out;
	for (auto &kv : reg_.mods()) {
		Dictionary m;
		m["version"] = G(kv.second.wanted_version);
		PackedStringArray decl;
		for (auto &c : kv.second.declared) decl.push_back(G(c));
		m["declared"] = decl;
		Dictionary calls, refusals;
		for (auto &c : kv.second.calls) calls[G(c.first)] = int64_t(c.second);
		for (auto &c : kv.second.refusals) refusals[G(c.first)] = int64_t(c.second);
		m["calls"] = calls; m["refusals"] = refusals;
		out[G(kv.first)] = m;
	}
	return out;
}

Dictionary JVModAPI::get_status() const {
	Dictionary d;
	d["version"] = get_api_version();
	d["strict"] = strict_;
	d["mode"] = get_mode();
	Dictionary open;
	for (int i = 0; i < 6; i++) open[kAccess[i]] = is_open(kAccess[i]);
	d["open"] = open;
	d["mods"] = get_mods();
	d["coverage"] = get_coverage();
	return d;
}

Dictionary JVModAPI::register_mod(const String &name, const String &version, const PackedStringArray &caps) {
	std::vector<std::string> c;
	for (int i = 0; i < caps.size(); i++) c.push_back(S(caps[i]));
	A::RegisterResult r = reg_.register_mod(S(name), S(version), c);
	Dictionary d;
	d["ok"] = r.ok; d["reason"] = G(r.reason);
	PackedStringArray g, dn;
	for (auto &s : r.granted) g.push_back(G(s));
	for (auto &s : r.denied) dn.push_back(G(s));
	d["granted"] = g; d["denied"] = dn;

	const String sig = version + String("|") + String(",").join(g) + String("|") + String(",").join(dn);
	const bool same = r.ok && reg_log_.count(name) && reg_log_[name] == sig;
	if (r.ok) reg_log_[name] = sig;
	if (r.ok && !same) jvm::log("api", "registered " + name + " (API " + version + "): " + String::num_int64(g.size()) + " capabilities" + (dn.is_empty() ? String() : ", refused " + String(", ").join(dn)));
	else if (!r.ok) jvm::warn("api", "registration refused for " + name + ": " + G(r.reason));
	return d;
}

bool JVModAPI::library_allowed(const String &short_name) {
	load_strict();
	std::string why;
	bool ok = A::library_allowed(S(short_name), strict_, &why);
	if (!ok) jvm::log("api", G(why));
	return ok;
}


Dictionary JVModAPI::load_config(const String &mod, const String &category) {
	if (!use(mod, "config.files")) return Dictionary();
	return jvm::load_category_config(category);
}


Node *JVModAPI::get_current_scene(const String &mod) {
	if (!use(mod, "scene.current")) return nullptr;
	return jvm::current_scene(this);
}
static Array by_class(Node *from, const char *cls) {
	Dictionary spec; spec["script_class"] = cls;
	return Array(jvm::find_targets(from, spec));
}
Array JVModAPI::find_balls(const String &mod) { return use(mod, "scene.balls") ? tracked_balls() : Array(); }

static void collect_balls(Node *n, std::vector<ObjectID> &out, int depth = 0) {
	if (!n || depth > 128) return;
	if (jvm::script_is(n, "Ball")) out.push_back(ObjectID(n->get_instance_id()));
	for (int i = 0; i < n->get_child_count(); i++) collect_balls(n->get_child(i), out, depth + 1);
}
void JVModAPI::_on_node_added_ball(Node *n) {
	if (balls_seeded_ && jvm::script_is(n, "Ball")) balls_.push_back(ObjectID(n->get_instance_id()));
}



Array JVModAPI::tracked_balls() {
	Array out;
	if (!is_inside_tree()) return out;
	SceneTree *t = get_tree();
	if (!balls_seeded_) {
		balls_seeded_ = true;
		if (!t->is_connected("node_added", callable_mp(this, &JVModAPI::_on_node_added_ball)))
			t->connect("node_added", callable_mp(this, &JVModAPI::_on_node_added_ball));
		collect_balls(t->get_root(), balls_);
	}
	Node *scene = t->get_current_scene();
	if (!scene) return out;
	std::vector<Node *> live;
	for (size_t i = 0; i < balls_.size();) {
		Node *n = Object::cast_to<Node>(ObjectDB::get_instance(balls_[i]));
		if (!n) { balls_.erase(balls_.begin() + i); continue; }
		bool dup = false;
		for (size_t k = 0; k < i; k++) if (balls_[k] == balls_[i]) { dup = true; break; }
		if (dup) { balls_.erase(balls_.begin() + i); continue; }
		i++;
		if (!n->is_inside_tree() || !jvm::script_is(n, "Ball")) continue;
		int d = 0;
		Node *a = n;
		for (; a && a != scene && d <= 64; a = a->get_parent()) d++;
		if (a != scene || d > 64) continue;
		live.push_back(n);
	}
	std::sort(live.begin(), live.end(), [](Node *x, Node *y) { return y->is_greater_than(x); });
	for (Node *n : live) out.push_back(n);
	return out;
}
Array JVModAPI::find_players(const String &mod) { return use(mod, "scene.players") ? by_class(this, "Player") : Array(); }
bool JVModAPI::is_ball(const String &mod, Object *o) { return use(mod, "scene.balls") && jvm::script_is(o, "Ball"); }

Node *JVModAPI::get_local_player(const String &mod) {
	if (!use(mod, "scene.players")) return nullptr;
	Dictionary spec; spec["scene_property"] = "player";
	Array a = Array(jvm::find_targets(this, spec));
	return a.is_empty() ? nullptr : Object::cast_to<Node>(a[0].operator Object *());
}

Object *JVModAPI::get_rally_for(const String &mod, Node *player) {
	if (!use(mod, "scene.rally")) return nullptr;
	return jvm::owner_property(player, "_rally", "Rally");
}


static Node *net_node(Node *self) {
	if (!self->is_inside_tree()) return nullptr;
	return self->get_tree()->get_root()->get_node_or_null(NodePath("Net"));
}
Dictionary JVModAPI::get_net_state(const String &mod) {
	Dictionary d;
	d["found"] = false; d["active"] = false; d["is_host"] = false; d["lobby_id"] = int64_t(0); d["steam"] = false;
	if (!use(mod, "net.state")) return d;
	d["steam"] = Engine::get_singleton()->has_singleton("Steam");
	Node *net = net_node(this);
	if (!net) return d;
	d["found"] = true;
	d["active"] = bool(net->get("active"));
	d["is_host"] = bool(net->get("is_host"));
	Variant lid = net->get("steam_lobby_id");
	d["lobby_id"] = lid.get_type() == Variant::INT ? int64_t(lid) : int64_t(0);
	d["party_lan"] = bool(net->get("party_lan"));
	d["custom_lobby"] = bool(net->get("custom_lobby"));
	d["in_match"] = bool(net->get("in_match"));
	d["practice_live"] = bool(net->get("practice_live"));
	return d;
}
static Object *steam_if_lobby(Node *self, int64_t &lid, bool &host) {
	Engine *e = Engine::get_singleton();
	Node *net = net_node(self);
	if (!net || !e->has_singleton("Steam")) return nullptr;
	Variant v = net->get("steam_lobby_id");
	if (v.get_type() != Variant::INT || int64_t(v) == 0 || !bool(net->get("active"))) return nullptr;
	lid = int64_t(v);
	host = bool(net->get("is_host"));
	return e->get_singleton("Steam");
}
String JVModAPI::lobby_data_get(const String &mod, const String &key) {
	if (!use(mod, "net.lobby_data")) return String();
	int64_t lid = 0; bool host = false;
	Object *steam = steam_if_lobby(this, lid, host);
	return steam ? String(steam->call("getLobbyData", lid, key)) : String();
}
bool JVModAPI::lobby_data_set(const String &mod, const String &key, const String &value) {
	if (!key.begins_with("jvm_") || !use(mod, "net.lobby_data")) return false;
	int64_t lid = 0; bool host = false;
	Object *steam = steam_if_lobby(this, lid, host);
	if (!steam || !host) return false;
	steam->call("setLobbyData", lid, key, value);
	return true;
}
bool JVModAPI::lobby_data_clear(const String &mod, const String &key) {
	if (!key.begins_with("jvm_") || !use(mod, "net.lobby_data", false)) return false;
	int64_t lid = 0; bool host = false;
	Object *steam = steam_if_lobby(this, lid, host);
	if (!steam || !host) return false;
	steam->call("setLobbyData", lid, key, "");
	return true;
}




String JVModAPI::ball_physics_open(const String &mod, const String &path) {
	if (!use(mod, "ball.physics")) return "refused";
	const String p = path.is_empty() ? String("res://game/ball/ball_physics.tres") : path;
	if (!ResourceLoader::get_singleton()->exists(p)) return "missing";
	Ref<Resource> r = ResourceLoader::get_singleton()->load(p);
	if (r.is_null()) return "failed";
	phys_[mod] = r;
	return "";
}
void JVModAPI::ball_physics_close(const String &mod) { phys_.erase(mod); }
Variant JVModAPI::ball_physics_get(const String &mod, const String &prop) {
	auto it = phys_.find(mod);
	if (it == phys_.end() || it->second.is_null() || !use(mod, "ball.physics", false)) return Variant();
	return it->second->get(prop);
}
bool JVModAPI::ball_physics_set(const String &mod, const String &prop, const Variant &value) {
	auto it = phys_.find(mod);
	if (it == phys_.end() || it->second.is_null() || !use(mod, "ball.physics")) return false;
	if (it->second->get(prop).get_type() == Variant::NIL) return false;
	it->second->set(prop, value);
	return true;
}
bool JVModAPI::ball_physics_restore(const String &mod, const String &prop, const Variant &value) {
	auto it = phys_.find(mod);
	if (it == phys_.end() || it->second.is_null() || !use(mod, "ball.physics", false)) return false;
	it->second->set(prop, value);
	return true;
}
Node *JVModAPI::ball_get_model(const String &mod, Node *ball) {
	if (!ball || !use(mod, "ball.model")) return nullptr;
	return Object::cast_to<Node>(ball->get("_model").operator Object *());
}
Dictionary JVModAPI::ball_get_flight(const String &mod, Node *ball) {
	Dictionary d;
	if (!ball || !use(mod, "ball.flight")) return d;
	d["vel"] = ball->get("vel");
	d["curve"] = ball->get("curve");
	d["dive_g"] = ball->get("dive_g");
	d["strike_seq"] = ball->get("strike_seq");
	d["hit_by"] = ball->get("hit_by");
	return d;
}
bool JVModAPI::ball_set_flight(const String &mod, Node *ball, const Dictionary &d) {
	if (!ball || !use(mod, "ball.flight")) return false;
	static const char *kw[] = { "vel", "curve", "dive_g" };
	for (const char *k : kw) if (d.has(k)) ball->set(k, d[k]);
	return true;
}
bool JVModAPI::connect_ball_struck(const String &mod, Node *ball, const Callable &c, int64_t flags) {
	if (!ball || !ball->has_signal("struck") || !use(mod, "ball.struck")) return false;
	if (ball->is_connected("struck", c)) return true;
	return ball->connect("struck", c, flags) == OK;
}


static bool scale_cap(const String &cap) { return cap == "ball.scale" || cap == "player.scale"; }
Vector3 JVModAPI::node_get_scale(const String &mod, const String &cap, Node *n) {
	Node3D *n3 = Object::cast_to<Node3D>(n);
	if (!n3 || !scale_cap(cap) || !use(mod, S(cap).c_str(), false)) return Vector3(1, 1, 1);
	return n3->get_scale();
}
bool JVModAPI::node_set_scale(const String &mod, const String &cap, Node *n, const Vector3 &s) {
	Node3D *n3 = Object::cast_to<Node3D>(n);
	if (!n3 || !scale_cap(cap) || !use(mod, S(cap).c_str())) return false;
	n3->set_scale(s);
	return true;
}
bool JVModAPI::node_restore_scale(const String &mod, const String &cap, Node *n, const Vector3 &s) {
	Node3D *n3 = Object::cast_to<Node3D>(n);
	if (!n3 || !scale_cap(cap) || !use(mod, S(cap).c_str(), false)) return false;
	n3->set_scale(s);
	return true;
}


bool JVModAPI::connect_player_touched(const String &mod, Node *player, const Callable &c) {
	if (!player || !player->has_signal("touched") || !use(mod, "player.touched")) return false;
	if (player->is_connected("touched", c)) return true;
	return player->connect("touched", c) == OK;
}



Dictionary JVModAPI::rally_touch_state(const String &mod, Object *rally) {
	Dictionary d;
	if (!rally || !use(mod, "rules.touch_count")) return d;
	d["count"] = int64_t(rally->get("touch_count"));
	d["side"] = int64_t(rally->get("touch_side"));
	d["practice"] = bool(rally->get("practice"));
	Variant lt = rally->get("last_toucher");
	d["has_last_toucher"] = lt.get_type() == Variant::OBJECT && lt.operator Object *() != nullptr;
	return d;
}
bool JVModAPI::rally_set_touch_count(const String &mod, Object *rally, int64_t count) {
	if (!rally || !use(mod, "rules.touch_count")) return false;
	rally->set("touch_count", count);
	return true;
}

bool JVModAPI::rally_clear_last_toucher(const String &mod, Object *rally) {
	if (!rally || !use(mod, "rules.double_hit")) return false;
	rally->set("last_toucher", Variant());
	return true;
}

static Node *find_panel(Node *n, int depth) {
	if (!n || depth > 12) return nullptr;
	if (String(n->get_name()) == "ModifiersPanel" && jvm::script_is(n, "ModifiersPanel")) return n;
	for (int i = 0; i < n->get_child_count(); i++) if (Node *f = find_panel(n->get_child(i), depth + 1)) return f;
	return nullptr;
}
static String rows_meta(const String &mod) { return mod == "goofy" ? String("jvm_rows") : "jvm_rows_" + mod; }


Node *JVModAPI::modifiers_panel_find(const String &mod) {
	if (!use(mod, "rules.modifiers_panel", false)) return nullptr;



	Node *sc = jvm::current_scene(this);
	Node *panel = nullptr;
	if (sc && sc->has_method("_card_btn")) {
		Variant mv = sc->get("_modifiers");
		if (mv.get_type() == Variant::OBJECT) panel = Object::cast_to<Node>(mv.operator Object *());
	}
	if (!panel && panel_id_) panel = Object::cast_to<Node>(ObjectDB::get_instance(panel_id_));
	if (panel && !panel->is_inside_tree()) panel = nullptr;
	if (!panel && sc) panel = find_panel(sc, 0);
	panel_id_ = panel ? uint64_t(panel->get_instance_id()) : 0;
	return panel;
}

Variant JVModAPI::modifiers_panel_rows(const String &mod, Node *panel) {
	if (!panel || !use(mod, "rules.modifiers_panel", false)) return Variant();
	const String k = rows_meta(mod);
	return panel->has_meta(k) ? panel->get_meta(k) : Variant();
}


Dictionary JVModAPI::modifiers_panel_add_rows(const String &mod, Node *panel, const Array &defs, const Callable &on_changed) {
	Dictionary out;
	out["ok"] = false; out["rows"] = Array();
	if (!panel || !use(mod, "rules.modifiers_panel")) return out;
	const String k = rows_meta(mod);
	if (!panel->has_method("_entry_row") || !panel->has_method("_rule") || !panel->has_signal("changed")) {
		jvm::log(mod, "Modifiers panel found but _entry_row/_rule/changed missing -> rows skipped");
		panel->set_meta(k, Array());
		return out;
	}
	Dictionary existing = panel->get("_rows");
	Control *any = existing.is_empty() ? nullptr : Object::cast_to<Control>(existing.values()[0].operator Object *());
	Node *list = any && any->get_parent() ? any->get_parent()->get_parent() : nullptr;
	if (!list) { jvm::log(mod, "Modifiers panel list not found -> rows skipped"); panel->set_meta(k, Array()); return out; }
	Array made;
	for (int i = 0; i < defs.size(); i++) {
		Dictionary d = defs[i];
		Node *sep = Object::cast_to<Node>(panel->call("_rule").operator Object *());
		Node *row = Object::cast_to<Node>(panel->call("_entry_row", d.get("entry", Dictionary())).operator Object *());
		if (!sep || !row) continue;
		list->add_child(sep); list->add_child(row);
		if (row->get_child_count() > 0 && row->get_child(0)->get_child_count() > 1)
			if (Label *l = Object::cast_to<Label>(row->get_child(0)->get_child(1))) l->set_text(String(d.get("desc", "")));
		made.push_back(sep); made.push_back(row);
	}
	panel->set_meta(k, made);
	if (on_changed.is_valid() && !panel->is_connected("changed", on_changed)) panel->connect("changed", on_changed);
	out["ok"] = true; out["rows"] = made;
	return out;
}
void JVModAPI::modifiers_panel_show_rows(const String &mod, const Array &rows, bool show) {
	if (!use(mod, "rules.modifiers_panel", false)) return;
	for (int i = 0; i < rows.size(); i++) if (Control *c = Object::cast_to<Control>(rows[i].operator Object *())) c->set_visible(show);
}

void JVModAPI::modifiers_panel_set_values(const String &mod, Node *panel, const Dictionary &values) {
	if (!panel || !use(mod, "rules.modifiers_panel", false)) return;
	Dictionary ctl = panel->get("_rows");
	panel->set("_quiet", true);
	Array keys = values.keys();
	for (int i = 0; i < keys.size(); i++) {
		Object *o = ctl.get(keys[i], Variant()).operator Object *();
		if (!o) continue;
		if (CheckBox *cb = Object::cast_to<CheckBox>(o)) { if (cb->is_pressed() != bool(values[keys[i]])) cb->set_pressed(bool(values[keys[i]])); }
		else if (int(o->get("selected")) != int(values[keys[i]])) o->set("selected", int(values[keys[i]]));
	}
	panel->set("_quiet", false);
}


double JVModAPI::time_scale_get() const { return Engine::get_singleton()->get_time_scale(); }
bool JVModAPI::time_scale_set(const String &mod, double v) {
	if (!use(mod, "engine.time_scale")) return false;
	Engine::get_singleton()->set_time_scale(v);
	return true;
}
bool JVModAPI::time_scale_restore(const String &mod, double v) {
	if (!use(mod, "engine.time_scale", false)) return false;
	Engine::get_singleton()->set_time_scale(v);
	return true;
}
void JVModAPI::overrides_set_rules(const String &mod, const Array &rules) {
	if (!use(mod, "engine.overrides", false)) return;
	overrides_[mod].rules = rules;
}
int64_t JVModAPI::overrides_apply(const String &mod) {
	if (!use(mod, "engine.overrides")) return 0;
	return overrides_[mod].apply(this, mod);
}
void JVModAPI::overrides_revert(const String &mod) {
	auto it = overrides_.find(mod);
	if (it == overrides_.end() || !use(mod, "engine.overrides", false)) return;
	it->second.revert();
}




bool JVModAPI::is_rpc(Object *o, const String &method) {
	if (method == "rpc" || method == "rpc_id" || method == "rpc_config" || method == "set_multiplayer_authority") return true;
	Ref<Script> s = o->get_script();
	for (int d = 0; s.is_valid() && d < 16; d++) {
		Variant cfg = s->get_rpc_config();
		if (cfg.get_type() == Variant::DICTIONARY && Dictionary(cfg).has(method)) return true;
		s = s->get_base_script();
	}
	return false;
}
Variant JVModAPI::game_get(const String &mod, Object *o, const String &prop) {
	if (!o || !use(mod, "game.generic")) return Variant();
	return o->get_indexed(NodePath(prop));
}
bool JVModAPI::game_set(const String &mod, Object *o, const String &prop, const Variant &v) {
	if (!o || !use(mod, "game.generic")) return false;
	o->set_indexed(NodePath(prop), v);
	return true;
}
Variant JVModAPI::game_call(const String &mod, Object *o, const String &method, const Array &args) {
	if (!o || !use(mod, "game.generic")) return Variant();
	if (is_rpc(o, method)) { jvm::warn("api", "refused " + mod + " -> game_call " + method + ": game RPCs are never called by mods"); return Variant(); }
	if (!o->has_method(method)) return Variant();
	return o->callv(method, args);
}
bool JVModAPI::game_connect(const String &mod, Object *o, const String &signal, const Callable &c) {
	if (!o || !o->has_signal(signal) || !use(mod, "game.generic")) return false;
	if (o->is_connected(signal, c)) return true;
	return o->connect(signal, c) == OK;
}
