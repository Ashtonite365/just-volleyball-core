




#include "mod_api.hpp"
#include "settings_ui.hpp"
#include "../common/jvm_common.hpp"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector4.hpp>

using namespace godot;
namespace A = jvm::api;
namespace GS = jvm::gs;

static String G(const std::string &s) { return String::utf8(s.c_str()); }
static std::string S(const String &s) { return std::string(s.utf8().get_data()); }
static const char *kCap = "gameplay.settings";
static const char *kSyncCap = "gameplay.settings_sync";


static bool is_num(const Variant &v) { return v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT; }

static bool to_value(const GS::Def &d, const Variant &v, GS::Value &out, String *why) {
	auto no = [&](const String &w) { if (why) *why = w; return false; };
	out.clear();
	switch (d.type) {
		case GS::Type::Bool:
			if (v.get_type() == Variant::BOOL) { out.push_back(bool(v) ? 1.0 : 0.0); return true; }
			if (is_num(v)) { out.push_back(double(v) != 0.0 ? 1.0 : 0.0); return true; }
			return no("expects true / false");
		case GS::Type::Int:
		case GS::Type::Float:
			if (is_num(v)) { out.push_back(double(v)); return true; }
			return no("expects a number");
		case GS::Type::Enum:
			if (v.get_type() == Variant::STRING || v.get_type() == Variant::STRING_NAME) {
				std::string s = S(String(v));
				for (size_t i = 0; i < d.options.size(); i++) if (d.options[i] == s) { out.push_back(double(i)); return true; }
				return no("unknown option '" + String(v) + "'");
			}
			if (v.get_type() == Variant::INT) { out.push_back(double(int64_t(v))); return true; }
			return no("expects one of the option names");
		case GS::Type::Vec:
			switch (v.get_type()) {
				case Variant::VECTOR2: { Vector2 x = v; out = { x.x, x.y }; return true; }
				case Variant::VECTOR3: { Vector3 x = v; out = { x.x, x.y, x.z }; return true; }
				case Variant::VECTOR4: { Vector4 x = v; out = { x.x, x.y, x.z, x.w }; return true; }
				case Variant::ARRAY: {
					Array a = v;
					for (int i = 0; i < a.size(); i++) { if (!is_num(a[i])) return no("vector components must be numbers"); out.push_back(double(a[i])); }
					return true;
				}
				case Variant::INT: case Variant::FLOAT: out.push_back(double(v)); return true;
				default: return no("expects a vector / array of numbers");
			}
	}
	return no("unknown type");
}

static Variant to_variant(const GS::Def &d, const GS::Value &v) {
	if (v.empty()) return Variant();
	switch (d.type) {
		case GS::Type::Bool: return v[0] >= 0.5;
		case GS::Type::Int: return int64_t(v[0]);
		case GS::Type::Float: return v[0];
		case GS::Type::Enum: { size_t i = size_t(v[0]); return i < d.options.size() ? Variant(G(d.options[i])) : Variant(); }
		case GS::Type::Vec:
			if (d.dim == 2 && v.size() >= 2) return Vector2(v[0], v[1]);
			if (d.dim == 3 && v.size() >= 3) return Vector3(v[0], v[1], v[2]);
			if (v.size() >= 4) return Vector4(v[0], v[1], v[2], v[3]);
			return Variant();
	}
	return Variant();
}

static double num_of(const Dictionary &d, const char *k, double dflt) { Variant v = d.get(k, Variant()); return is_num(v) ? double(v) : dflt; }


static bool def_from_dict(const String &mod, const Dictionary &in, GS::Def &d, String &sync, Callable &on_change, Callable &on_read, String &why) {
	d = GS::Def();
	d.mod = S(mod);
	d.id = S(String(in.get("id", "")));
	String type = in.get("type", "float");
	if (!GS::parse_type(S(type), d.type)) { why = G(d.id) + ": unknown type '" + type + "' (bool, int, float, enum, vec)"; return false; }
	d.dim = d.type == GS::Type::Vec ? int(num_of(in, "dim", 0)) : 1;
	Array opts = in.get("options", Array());
	for (int i = 0; i < opts.size(); i++) d.options.push_back(S(String(opts[i])));
	d.min = num_of(in, "min", d.type == GS::Type::Float || d.type == GS::Type::Vec ? 0.0 : 0.0);
	d.max = num_of(in, "max", d.type == GS::Type::Float ? 1.0 : (d.type == GS::Type::Int ? 100.0 : 1.0));
	d.step = num_of(in, "step", 0.0);
	d.label = S(String(in.get("label", "")));
	d.tooltip = S(String(in.get("tooltip", "")));
	d.category = S(String(in.get("category", "")));
	sync = in.get("sync", "");
	on_change = in.get("on_change", Callable());
	on_read = in.get("on_read", Callable());
	Variant dv = in.get("default", Variant());
	if (d.type == GS::Type::Vec && d.dim == 0) {
		if (dv.get_type() == Variant::VECTOR2) d.dim = 2; else if (dv.get_type() == Variant::VECTOR3) d.dim = 3; else if (dv.get_type() == Variant::VECTOR4) d.dim = 4;
		else if (dv.get_type() == Variant::ARRAY) d.dim = int(Array(dv).size());
	}
	if (dv.get_type() == Variant::NIL) { why = G(d.id) + ": default is required"; return false; }

	GS::Def probe = d;
	if (probe.type == GS::Type::Enum) { probe.min = 0; probe.max = double(probe.options.size()) - 1; }
	String w;
	if (!to_value(probe, dv, d.def, &w)) { why = G(d.id) + ": default " + w; return false; }
	return true;
}


String JVModAPI::gs_state_path(const String &mod) const {
	String safe = mod.validate_filename().replace(" ", "_");
	return jvm::mods_root().path_join("api").path_join("state").path_join("gameplay").path_join(safe + String(".json"));
}

bool JVModAPI::gs_save(const String &mod) {
	Dictionary vals;
	for (auto &id : gs_.ids()) {
		const GS::Def *d = gs_.find(id);
		if (!d || G(d->mod) != mod) continue;
		if (const GS::Value *v = gs_.local(id)) vals[G(id)] = to_variant(*d, *v);
	}
	Dictionary f;
	auto it = gs_versions_.find(mod);
	f["version"] = it == gs_versions_.end() ? int64_t(1) : it->second;
	f["values"] = vals;
	String p = gs_state_path(mod);
	DirAccess::make_dir_recursive_absolute(p.get_base_dir());
	Ref<FileAccess> fa = FileAccess::open(p + String(".tmp"), FileAccess::WRITE);
	if (fa.is_null()) return false;
	fa->store_string(JSON::stringify(f, "  "));
	fa->close();
	return DirAccess::rename_absolute(p + String(".tmp"), p) == OK;
}




Dictionary JVModAPI::gs_load(const String &mod, int64_t version, const Callable &migrate) {
	Dictionary out;
	String p = gs_state_path(mod);
	Dictionary f;
	if (FileAccess::file_exists(p)) { Variant v = jvm::load_json(p); if (v.get_type() == Variant::DICTIONARY) f = v; else jvm::warn("api", "gameplay settings: unreadable " + p + " (ignored)"); }
	int64_t from = is_num(f.get("version", Variant())) ? int64_t(f["version"]) : 0;
	Dictionary values = f.get("values", Dictionary());
	bool migrated = false;
	if (from < version && migrate.is_valid()) {
		Variant r = migrate.call(from, values.duplicate(true));
		if (r.get_type() == Variant::DICTIONARY) { values = r; migrated = true; }
		else jvm::warn("api", "gameplay settings: " + mod + " migrate() did not return a Dictionary (stored values kept)");
	} else if (from > version) {
		jvm::warn("api", "gameplay settings: " + mod + " state is schema " + String::num_int64(from) + ", mod is " + String::num_int64(version) + " (values that still validate are kept)");
	}
	out["values"] = values; out["from"] = from; out["migrated"] = migrated || (from != version && from != 0);
	return out;
}


Variant JVModAPI::gs_read(const String &id) {
	std::string sid = S(id), src;
	const GS::Def *d = gs_.find(sid);
	if (!d) return Variant();
	GS::Value v = gs_.get(sid, &src);
	Variant out = to_variant(*d, v);
	if (gs_.role() == GS::Role::Off) return out;
	auto h = gs_hooks_.find(id);
	if (h != gs_hooks_.end() && h->second.on_read.is_valid() && !gs_in_read_) {
		gs_in_read_ = true;
		Variant r = h->second.on_read.call(id, out, G(src));
		gs_in_read_ = false;
		GS::Value rv, sv;
		if (r.get_type() != Variant::NIL && to_value(*d, r, rv, nullptr) && GS::sanitize(*d, rv, sv)) out = to_variant(*d, sv);
	}
	return out;
}

void JVModAPI::gs_refresh() {
	gs_dirty_ = false;
	std::vector<std::pair<String, Variant>> fired;
	for (auto &sid : gs_.ids()) {
		String id = G(sid);
		Variant v = gs_read(id);
		auto it = gs_effective_.find(id);
		if (it == gs_effective_.end()) { gs_effective_[id] = v; continue; }
		if (it->second == v) continue;
		Variant prev = it->second;
		it->second = v;
		std::string src; gs_.get(sid, &src);
		const GS::Def *d = gs_.find(sid);
		Callable cb;
		auto h = gs_hooks_.find(id);
		if (h != gs_hooks_.end() && h->second.on_change.is_valid()) cb = h->second.on_change;
		else if (d) { auto m = gs_mod_change_.find(G(d->mod)); if (m != gs_mod_change_.end() && m->second.is_valid()) cb = m->second; }
		if (cb.is_valid()) cb.call(id, v, G(src));
		Dictionary ev; ev["id"] = id; ev["value"] = v; ev["previous"] = prev; ev["source"] = G(src);
		emit_event("gameplay_setting_changed", ev);
		emit_signal("gameplay_setting_changed", id, v, G(src));
	}
	for (auto it = gs_effective_.begin(); it != gs_effective_.end();) { if (!gs_.find(S(it->first))) it = gs_effective_.erase(it); else ++it; }
}

void JVModAPI::gs_update_role() {
	A::GateView g = view();
	GS::Role r = GS::Role::Off;
	if (A::access_open(A::Access::Gameplay, g)) {
		r = GS::Role::Local;
		if (A::is_sync_mode(g.mode) && is_inside_tree()) {
			Node *net = get_tree()->get_root()->get_node_or_null(NodePath("Net"));
			if (net && bool(net->get("active")) && !bool(net->get("is_host"))) r = GS::Role::Client;
		}
	}
	if (r == gs_.role()) return;
	GS::Role prev = gs_.role();
	gs_.set_role(r);
	if (r == GS::Role::Client) gs_peer_names_.clear();
	jvm::log("api", String("gameplay settings: role ") + GS::to_string(prev) + " -> " + GS::to_string(r));
	Dictionary ev; ev["role"] = GS::to_string(r); ev["previous"] = GS::to_string(prev);
	emit_event("gameplay_role_changed", ev);
	gs_dirty_ = true;
}

void JVModAPI::gs_tick(double delta) {
	gs_lobby_tick(delta);
	if (gs_.ids().empty()) return;
	gs_update_role();
	if (gs_dirty_) gs_refresh();
}


Dictionary JVModAPI::gameplay_register(const String &mod, const Array &defs, const Dictionary &options) {
	Dictionary res;
	Array registered, errors;
	res["ok"] = false;
	if (!use(mod, kCap, false)) { errors.push_back("refused: register_mod with gameplay.settings first"); res["errors"] = errors; return res; }
	int64_t version = is_num(options.get("version", Variant())) ? int64_t(options["version"]) : 1;
	if (version < 1) version = 1;
	Callable migrate = options.get("migrate", Callable());
	Callable mod_change = options.get("on_change", Callable());
	for (int i = 0; i < defs.size(); i++) {
		if (defs[i].get_type() != Variant::DICTIONARY) { errors.push_back("entry " + String::num_int64(i) + " is not a Dictionary"); continue; }
		GS::Def d; String sync, why; Callable oc, orr;
		if (!def_from_dict(mod, defs[i], d, sync, oc, orr, why)) { errors.push_back(why); continue; }
		GS::Result r = gs_.add(d, S(sync));
		if (!r.ok) { errors.push_back(G(r.error)); continue; }
		GsHooks hk; hk.on_change = oc; hk.on_read = orr;
		gs_hooks_[G(d.id)] = hk;
		registered.push_back(G(d.id));
	}
	if (registered.is_empty()) {
		res["ok"] = false; res["registered"] = registered; res["errors"] = errors; res["version"] = version;
		res["migrated_from"] = 0; res["loaded"] = 0; res["role"] = GS::to_string(gs_.role());
		jvm::warn("api", "gameplay settings: " + mod + " registered nothing" + (errors.is_empty() ? String() : ": " + String(errors[0])));
		return res;
	}


	if (mod_change.is_valid()) gs_mod_change_[mod] = mod_change;
	gs_versions_[mod] = version;
	if (options.has("mod_version")) gs_mod_versions_[mod] = String(options["mod_version"]);

	Dictionary ld = gs_load(mod, version, migrate);
	Dictionary vals = ld["values"];
	Array keys = vals.keys();
	int loaded = 0;
	for (int i = 0; i < keys.size(); i++) {
		String id = keys[i];
		const GS::Def *d = gs_.find(S(id));
		if (!d || G(d->mod) != mod) { jvm::log("api", "gameplay settings: " + mod + " stored value for unknown setting " + id + " dropped"); continue; }
		GS::Value v; String why;
		if (!to_value(*d, vals[id], v, &why) || !gs_.set_local(S(id), v).ok) { jvm::warn("api", "gameplay settings: " + id + " stored value invalid (" + why + "), default used"); continue; }
		loaded++;
	}
	if (bool(ld["migrated"]) || int64_t(ld["from"]) != version) gs_save(mod);
	for (int i = 0; i < registered.size(); i++) gs_effective_.erase(registered[i]);
	gs_update_role();
	gs_refresh();
	res["ok"] = errors.is_empty() && !registered.is_empty();
	res["registered"] = registered;
	res["errors"] = errors;
	res["version"] = version;
	res["migrated_from"] = ld["from"];
	res["loaded"] = loaded;
	res["role"] = GS::to_string(gs_.role());
	jvm::log("api", "gameplay settings: " + mod + " registered " + String::num_int64(registered.size()) + " setting(s), schema " + String::num_int64(version) +
			(errors.is_empty() ? String() : ", " + String::num_int64(errors.size()) + " refused: " + String(errors[0])));
	return res;
}

Dictionary JVModAPI::gameplay_register_setting(const String &mod, const Dictionary &def) {
	Array a; a.push_back(def);
	Dictionary opts;
	auto it = gs_versions_.find(mod);
	opts["version"] = it == gs_versions_.end() ? int64_t(1) : it->second;
	return gameplay_register(mod, a, opts);
}

int64_t JVModAPI::gameplay_unregister(const String &mod) {
	if (!use(mod, kCap, false)) return 0;
	std::vector<String> ids;
	for (auto &id : gs_.ids()) if (G(gs_.find(id)->mod) == mod) ids.push_back(G(id));
	int n = gs_.remove_mod(S(mod));
	for (auto &id : ids) { gs_hooks_.erase(id); gs_effective_.erase(id); }
	gs_mod_change_.erase(mod);
	return n;
}


Variant JVModAPI::gameplay_get(const String &mod, const String &id) {
	if (!use(mod, kCap, false)) return Variant();
	return gs_read(id);
}

Dictionary JVModAPI::gameplay_get_all(const String &mod, const String &owner) {
	Dictionary out;
	if (!use(mod, kCap, false)) return out;
	for (auto &id : gs_.ids()) if (owner.is_empty() || G(gs_.find(id)->mod) == owner) out[G(id)] = gs_read(G(id));
	return out;
}

static Dictionary set_result(bool ok, const String &err) { Dictionary d; d["ok"] = ok; d["error"] = err; return d; }

Dictionary JVModAPI::gs_ui_set(const String &id, const Variant &value) {
	const GS::Def *d = gs_.find(S(id));
	if (!d) return set_result(false, "unknown setting " + id);
	gs_update_role();
	if (gs_.locked()) return set_result(false, gs_.role() == GS::Role::Client ? "the host controls gameplay settings in this lobby" : "gameplay settings are locked for this match (decided at match start)");
	GS::Value v; String why;
	if (!to_value(*d, value, v, &why)) return set_result(false, id + String(": ") + why);
	GS::Result r = gs_.set_local(S(id), v);
	if (!r.ok) return set_result(false, G(r.error));
	gs_save(G(d->mod));
	gs_refresh();
	Dictionary res = set_result(true, "");
	const GS::Value *lv = gs_.local(S(id));
	res["value"] = to_variant(*d, lv ? *lv : d->def);
	res["effective"] = gs_read(id);
	return res;
}

Dictionary JVModAPI::gameplay_set(const String &mod, const String &id, const Variant &value) {
	if (!use(mod, kCap, false)) return set_result(false, "refused");
	const GS::Def *d = gs_.find(S(id));
	if (d && G(d->mod) != mod) return set_result(false, id + String(" belongs to ") + G(d->mod) + String(" (players edit it in Settings > mods)"));
	return gs_ui_set(id, value);
}

bool JVModAPI::gs_ui_reset(const String &id) {
	const GS::Def *d = gs_.find(S(id));
	if (!d) return false;
	gs_update_role();
	if (gs_.locked()) return false;
	gs_.reset_local(S(id));
	gs_save(G(d->mod));
	gs_refresh();
	return true;
}
bool JVModAPI::gameplay_reset(const String &mod, const String &id) {
	if (!use(mod, kCap, false)) return false;
	const GS::Def *d = gs_.find(S(id));
	if (!d || G(d->mod) != mod) return false;
	return gs_ui_reset(id);
}

Dictionary JVModAPI::gameplay_info(const String &mod, const String &id) {
	Dictionary o;
	if (!use(mod, kCap, false)) return o;
	const GS::Def *d = gs_.find(S(id));
	if (!d) return o;
	std::string src; gs_.get(d->id, &src);
	o["id"] = id; o["mod"] = G(d->mod); o["type"] = GS::to_string(d->type); o["dim"] = d->dim;
	o["default"] = to_variant(*d, d->def); o["min"] = d->min; o["max"] = d->max; o["step"] = d->step;
	PackedStringArray opts; for (auto &x : d->options) opts.push_back(G(x));
	o["options"] = opts;
	o["label"] = G(d->label); o["tooltip"] = G(d->tooltip); o["category"] = G(d->category); o["order"] = d->order;
	const GS::Value *lv = gs_.local(d->id);
	o["local"] = to_variant(*d, lv ? *lv : d->def);
	o["value"] = gs_read(id);
	o["source"] = G(src);
	o["locked"] = gs_.locked();
	o["role"] = GS::to_string(gs_.role());
	o["sync"] = "gameplay";
	return o;
}

Array JVModAPI::gs_ui_list() {
	Array out;
	for (auto &id : gs_.ids()) {
		const GS::Def *d = gs_.find(id);
		Dictionary o = gameplay_info(G(d->mod), G(id));
		if (o.is_empty()) { o["id"] = G(id); }
		out.push_back(o);
	}
	return out;
}
Array JVModAPI::gameplay_list(const String &mod) {
	if (!use(mod, kCap, false)) return Array();
	return gs_ui_list();
}
String JVModAPI::gameplay_role(const String &mod) {
	if (!use(mod, kCap, false)) return String();
	gs_update_role();
	return GS::to_string(gs_.role());
}
uint64_t JVModAPI::gs_ui_host_hash() const {
	std::string b = GS::Registry::encode(gs_.host_values());
	for (auto &m : gs_.host_disabled()) b += "!" + m;
	if (gs_.host_mods_known()) for (auto &kv : gs_.host_mods()) b += "+" + kv.first + "=" + kv.second;
	return GS::Registry::hash(b);
}


static String hex64(uint64_t h) { return String::num_uint64(h, 16).lpad(16, "0"); }

Dictionary JVModAPI::gameplay_snapshot(const String &mod) {
	Dictionary o;
	o["ok"] = false;
	if (!use(mod, kSyncCap)) { o["reason"] = why_not(mod, kSyncCap); return o; }
	gs_update_role();
	if (gs_.role() != GS::Role::Local) { o["reason"] = "only the host exports (role " + String(GS::to_string(gs_.role())) + ")"; return o; }
	Dictionary vals;
	auto snap = gs_.snapshot();
	for (auto &kv : snap) vals[G(kv.first)] = to_variant(*gs_.find(kv.first), kv.second);
	std::string bytes = GS::Registry::encode(snap);
	o["ok"] = true; o["version"] = GS::kSnapshotVersion; o["values"] = vals;
	o["hash"] = hex64(GS::Registry::hash(bytes)); o["bytes"] = int64_t(bytes.size());
	return o;
}

PackedByteArray JVModAPI::gameplay_snapshot_bytes(const String &mod) {
	PackedByteArray out;
	if (!use(mod, kSyncCap)) return out;
	gs_update_role();
	if (gs_.role() != GS::Role::Local) return out;
	std::string b = GS::Registry::encode(gs_.snapshot());
	out.resize(int64_t(b.size()));
	for (size_t i = 0; i < b.size(); i++) out.set(int64_t(i), uint8_t(b[i]));
	return out;
}

Dictionary JVModAPI::gameplay_apply_snapshot(const String &mod, const Variant &payload) {
	Dictionary o;
	o["ok"] = false;
	if (!use(mod, kSyncCap)) { o["reason"] = why_not(mod, kSyncCap); return o; }
	gs_update_role();
	if (gs_.role() == GS::Role::Local) { o["reason"] = "this player is the host (or offline): its own values are authoritative"; return o; }
	GS::ApplyResult r;
	if (payload.get_type() == Variant::PACKED_BYTE_ARRAY || payload.get_type() == Variant::STRING) {
		PackedByteArray b = payload.get_type() == Variant::STRING ? String(payload).to_utf8_buffer() : PackedByteArray(payload);
		if (b.size() > GS::kSnapshotMaxBytes) { o["reason"] = "payload larger than 32 KiB"; return o; }
		std::string s(reinterpret_cast<const char *>(b.ptr()), size_t(b.size()));
		r = gs_.apply_host(s);
	} else if (payload.get_type() == Variant::DICTIONARY) {
		Dictionary pd = payload;
		Dictionary vals = pd.has("values") ? Dictionary(pd["values"]) : pd;
		if (is_num(pd.get("version", Variant())) && int64_t(pd["version"]) != GS::kSnapshotVersion) { o["reason"] = "unsupported snapshot version"; return o; }
		std::map<std::string, GS::Value> raw;
		Array keys = vals.keys();
		int bad = 0;
		if (keys.size() > GS::kSnapshotMaxEntries) { o["reason"] = "too many entries"; return o; }
		for (int i = 0; i < keys.size(); i++) {
			std::string id = S(String(keys[i]));
			const GS::Def *d = gs_.find(id);
			GS::Value v;
			if (!d) { raw[id] = GS::Value{ 0 }; continue; }
			if (!to_value(*d, vals[keys[i]], v, nullptr)) { bad++; continue; }
			raw[id] = v;
		}
		r = gs_.apply_host_values(raw);
		r.rejected += bad;
	} else { o["reason"] = "payload must be PackedByteArray (JVGS bytes) or Dictionary {values}"; return o; }
	if (!r.ok) { o["reason"] = r.errors.empty() ? String("rejected") : G(r.errors[0]); return o; }
	o["ok"] = true; o["applied"] = r.applied; o["rejected"] = r.rejected; o["unknown"] = r.unknown;
	o["hash"] = hex64(gs_ui_host_hash());
	gs_refresh();
	return o;
}

bool JVModAPI::gameplay_clear_host(const String &mod) {
	if (!use(mod, kSyncCap, false)) return false;
	gs_.clear_host();
	gs_refresh();
	return true;
}


static Variant rules_const(const char *name, const Variant &dflt) {

	TypedArray<Dictionary> gl = ProjectSettings::get_singleton()->get_global_class_list();
	for (int i = 0; i < gl.size(); i++) {
		Dictionary d = gl[i];
		if (String(d.get("class", "")) != "Rules") continue;
		Ref<Script> sc = ResourceLoader::get_singleton()->load(String(d.get("path", "")));
		if (sc.is_null()) return dflt;
		Dictionary m = sc->get_script_constant_map();
		return m.get(name, dflt);
	}
	return dflt;
}

Dictionary JVModAPI::game_rules_info(const String &mod) {
	Dictionary o;
	if (!use(mod, "game.rules", false)) return o;
	o["touches_per_side"] = rules_const("TOUCHES_PER_SIDE", int64_t(3));
	o["consecutive_per_player"] = int64_t(1);
	o["double_hit"] = "bar";
	o["solo_side_exempt"] = true;
	o["over_team_limit"] = "fault";
	o["block_counts"] = false;
	o["practice_exempt"] = true;
	Ref<Resource> feel = io_resource(mod, "game.rules", "res://game/player/player_feel.tres");
	o["block_classic"] = feel.is_valid() ? feel->get("block_classic") : Variant(false);
	return o;
}

Dictionary JVModAPI::game_rules_state(const String &mod, Object *rally) {
	Dictionary d;
	if (!rally || !use(mod, "game.rules", false)) return d;
	d["count"] = int64_t(rally->get("touch_count"));
	d["side"] = int64_t(rally->get("touch_side"));
	d["practice"] = bool(rally->get("practice"));
	Variant lt = rally->get("last_toucher");
	Object *o = lt.get_type() == Variant::OBJECT ? lt.operator Object *() : nullptr;
	d["has_last_toucher"] = o != nullptr;
	d["last_toucher_id"] = o ? int64_t(o->get_instance_id()) : int64_t(0);
	return d;
}

void JVModAPI::gs_attach_ui() {
	if (get_node_or_null(NodePath("JVSettingsUI"))) return;
	JVSettingsUI *ui = memnew(JVSettingsUI);
	ui->set_name("JVSettingsUI");
	add_child(ui);
}


void JVModAPI::bind_gameplay_settings_methods() {
#define BM(name, ...) ClassDB::bind_method(D_METHOD(#name, ##__VA_ARGS__), &JVModAPI::name)
	ClassDB::bind_method(D_METHOD("gameplay_register", "mod", "defs", "options"), &JVModAPI::gameplay_register, DEFVAL(Dictionary()));
	BM(gameplay_register_setting, "mod", "def"); BM(gameplay_unregister, "mod");
	BM(gameplay_get, "mod", "id");
	ClassDB::bind_method(D_METHOD("gameplay_get_all", "mod", "owner"), &JVModAPI::gameplay_get_all, DEFVAL(""));
	BM(gameplay_set, "mod", "id", "value"); BM(gameplay_reset, "mod", "id");
	BM(gameplay_list, "mod"); BM(gameplay_info, "mod", "id"); BM(gameplay_role, "mod");
	BM(gameplay_snapshot, "mod"); BM(gameplay_snapshot_bytes, "mod"); BM(gameplay_apply_snapshot, "mod", "payload"); BM(gameplay_clear_host, "mod");
	BM(game_rules_info, "mod"); BM(game_rules_state, "mod", "rally");
	BM(gameplay_manifest, "mod"); BM(gameplay_manifest_bytes, "mod");
	ClassDB::bind_method(D_METHOD("gameplay_set_peer_manifest", "mod", "player_id", "payload", "player_name"), &JVModAPI::gameplay_set_peer_manifest, DEFVAL(""));
	BM(gameplay_remove_peer, "mod", "player_id"); BM(gameplay_set_host_manifest, "mod", "payload");
	ClassDB::bind_method(D_METHOD("gameplay_mod_active_for", "mod", "mod_id", "player_id"), &JVModAPI::gameplay_mod_active_for, DEFVAL(""));
	BM(gameplay_player_mods, "mod"); BM(gameplay_match_start, "mod"); BM(gameplay_match_end, "mod"); BM(gameplay_match_status, "mod");
#undef BM
	ADD_SIGNAL(MethodInfo("gameplay_setting_changed", PropertyInfo(Variant::STRING, "id"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::STRING, "source")));
}









static bool never_in_manifest(const std::string &m) {
	return m.rfind("core_", 0) == 0 || m == "textures" || m == "models" || m == "loader" || m == "jvm_loader" || m == "menu_ui" || m == "menu_perf" || m == "picker";
}

GS::Manifest JVModAPI::gs_build_manifest() const {
	GS::Manifest m;
	for (auto &kv : reg_.mods()) {
		if (never_in_manifest(kv.first)) continue;
		const String name = G(kv.first);
		bool gameplay = gs_versions_.count(name) > 0;
		for (auto &c : kv.second.declared) {
			const A::Capability *cap = A::find(c);
			if (cap && (cap->access == A::Access::Gameplay || cap->access == A::Access::Sync || cap->access == A::Access::Offline)) { gameplay = true; break; }
		}
		if (!gameplay) continue;
		auto mv = gs_mod_versions_.find(name);
		auto sv = gs_versions_.find(name);
		m[kv.first] = mv != gs_mod_versions_.end() ? S(mv->second) : sv != gs_versions_.end() ? std::to_string(sv->second) : kv.second.wanted_version;
	}
	return m;
}




bool JVModAPI::gs_match_veto(const String &mod, const char *cap) const {
	if (!gs_.match_locked() && !gs_.host_mods_known()) return false;
	const A::Capability *c = A::find(cap);
	if (!c || (c->access != A::Access::Gameplay && c->access != A::Access::Offline)) return false;
	if (String(cap).begins_with("gameplay.settings")) return false;
	if (!gs_.local_mods().count(S(mod))) return false;
	return gs_.match_disabled(S(mod));
}

bool JVModAPI::gs_loader_ok() const {
	Node *n = lib("loader");
	if (!n) return false;
	if (!n->has_method("get_status")) return true;
	Dictionary st = n->call("get_status");
	return String(st.get("state", "")) == "active";
}

void JVModAPI::gs_notice(const String &text) {
	jvm::warn("api", "gameplay settings: " + text);
	if (JVSettingsUI *ui = Object::cast_to<JVSettingsUI>(get_node_or_null(NodePath("JVSettingsUI")))) ui->show_notice(text);
}

static const char *kLoaderNote = "maps / court packs with built-in custom values need it, so they may play differently";

void JVModAPI::gs_lobby_tick(double delta) {
	if ((gs_lobby_t_ -= delta) > 0.0) return;
	gs_lobby_t_ = 0.5;
	gs_.set_local_mods(gs_build_manifest());
	A::GateView g = view();
	bool in_lobby = false;
	if (A::is_sync_mode(g.mode) && is_inside_tree()) {
		Node *net = get_tree()->get_root()->get_node_or_null(NodePath("Net"));
		in_lobby = net && bool(net->get("active"));
	}
	if (in_lobby && !gs_lobby_) { gs_lobby_ = true; gs_loader_checked_ = false; }
	else if (!in_lobby && gs_lobby_) {
		gs_lobby_ = false;
		gs_.clear_peers(); gs_.clear_host_mods(); gs_.unlock_match(); gs_peer_names_.clear();
		gs_dirty_ = true;
	}
	if (gs_lobby_ && !gs_loader_checked_) {
		gs_loader_checked_ = true;
		if (!gs_loader_ok()) gs_notice(String("your texture loader (jvm_loader) is not running: ") + kLoaderNote + ".");
	}
}

Dictionary JVModAPI::gs_missing_of(const GS::Manifest &theirs) {
	Dictionary out;
	for (auto &kv : gs_.local_mods()) {
		std::string why;
		if (!GS::mod_active(kv.first, kv.second, theirs, &why)) out[G(kv.first)] = G(why);
	}
	return out;
}

Dictionary JVModAPI::gs_disabled_dict() {
	Dictionary out;
	if (gs_.role() == GS::Role::Client) {
		for (auto &kv : gs_.local_mods()) if (gs_.match_disabled(kv.first)) {
			std::string why;
			gs_.mod_active_for(kv.first, "", &why);
			Array a; Dictionary e; e["player"] = ""; e["name"] = ""; e["why"] = G(why); a.push_back(e);
			out[G(kv.first)] = a;
		}
		for (auto &m : gs_.host_disabled()) if (!out.has(G(m))) {
			Array a; Dictionary e; e["player"] = ""; e["name"] = ""; e["why"] = "a player does not have it"; a.push_back(e);
			out[G(m)] = a;
		}
		return out;
	}
	for (auto &kv : gs_.disabled()) {
		Array a;
		for (auto &w : kv.second) {
			Dictionary e;
			const String id = G(w.player);
			e["player"] = id; e["name"] = gs_peer_names_.count(id) ? gs_peer_names_[id] : id; e["why"] = G(w.why);
			a.push_back(e);
		}
		out[G(kv.first)] = a;
	}
	return out;
}

static bool payload_text(const Variant &payload, std::string &out) {
	if (payload.get_type() == Variant::PACKED_BYTE_ARRAY) { PackedByteArray b = payload; out.assign(reinterpret_cast<const char *>(b.ptr()), size_t(b.size())); return true; }
	if (payload.get_type() == Variant::STRING) { out = S(String(payload)); return true; }
	return false;
}

Dictionary JVModAPI::gameplay_manifest(const String &mod) {
	Dictionary o;
	if (!use(mod, kCap, false)) return o;
	GS::Manifest m = gs_build_manifest();
	gs_.set_local_mods(m);
	const bool lo = gs_loader_ok();
	Dictionary mods;
	for (auto &kv : m) mods[G(kv.first)] = G(kv.second);
	std::set<std::string> off;
	if (gs_.role() == GS::Role::Local) for (auto &kv : gs_.disabled()) off.insert(kv.first);
	std::string b = GS::encode_manifest(m, lo, &off);
	o["mods"] = mods; o["loader_ok"] = lo; o["bytes"] = int64_t(b.size());
	return o;
}

PackedByteArray JVModAPI::gameplay_manifest_bytes(const String &mod) {
	PackedByteArray out;
	if (!use(mod, kSyncCap)) return out;
	gs_update_role();
	GS::Manifest m = gs_build_manifest();
	gs_.set_local_mods(m);
	std::set<std::string> off;
	if (gs_.role() == GS::Role::Local) for (auto &kv : gs_.disabled()) off.insert(kv.first);
	std::string b = GS::encode_manifest(m, gs_loader_ok(), &off);
	out.resize(int64_t(b.size()));
	for (size_t i = 0; i < b.size(); i++) out.set(int64_t(i), uint8_t(b[i]));
	return out;
}

Dictionary JVModAPI::gameplay_set_peer_manifest(const String &mod, const String &player_id, const Variant &payload, const String &player_name) {
	Dictionary o;
	o["ok"] = false;
	if (!use(mod, kSyncCap)) { o["error"] = why_not(mod, kSyncCap); return o; }
	gs_update_role();
	if (gs_.role() != GS::Role::Local) { o["error"] = "only the host collects mod lists (role " + String(GS::to_string(gs_.role())) + ")"; return o; }
	if (player_id.is_empty()) { o["error"] = "player_id required"; return o; }
	std::string text, why;
	GS::PeerInfo p;
	if (!payload_text(payload, text) || !GS::decode_manifest(text, p.mods, p.loader_ok, &why)) { o["error"] = "bad manifest: " + G(why.empty() ? std::string("expected bytes or text") : why); return o; }
	gs_.set_local_mods(gs_build_manifest());
	gs_.set_peer(S(player_id), p);
	const String name = player_name.is_empty() ? player_id : player_name;
	gs_peer_names_[player_id] = name;
	const bool late = gs_.is_late(S(player_id));
	Dictionary missing = gs_missing_of(p.mods);
	if (!missing.is_empty()) {
		String l;
		Array k = missing.keys();
		for (int i = 0; i < k.size(); i++) l += (i ? ", " : "") + String(k[i]) + " (" + String(missing[k[i]]) + ")";
		jvm::log("api", "gameplay settings: " + name + " does not have " + l +
				(late ? String(": joined mid-match, decided at the next match start") : String(": off for everyone from match start")));
	} else jvm::log("api", "gameplay settings: " + name + " has every gameplay mod of the host");
	if (!p.loader_ok) gs_notice(name + String("'s texture loader (jvm_loader) is not running: ") + kLoaderNote + " for them.");
	Dictionary mods;
	for (auto &kv : p.mods) mods[G(kv.first)] = G(kv.second);
	Dictionary ev;
	ev["player"] = player_id; ev["name"] = name; ev["mods"] = mods; ev["loader_ok"] = p.loader_ok; ev["missing"] = missing;
	ev["late"] = late; ev["locked"] = gs_.match_locked(); ev["disabled"] = gs_disabled_dict();
	emit_event("gameplay_mods_status", ev);
	o = ev.duplicate(); o["ok"] = true; o["error"] = "";
	return o;
}

bool JVModAPI::gameplay_remove_peer(const String &mod, const String &player_id) {
	if (!use(mod, kSyncCap, false)) return false;
	gs_peer_names_.erase(player_id);
	return gs_.remove_peer(S(player_id));
}

Dictionary JVModAPI::gameplay_match_start(const String &mod) {
	Dictionary o;
	o["ok"] = false;
	if (!use(mod, kSyncCap)) { o["error"] = why_not(mod, kSyncCap); return o; }
	gs_update_role();
	if (gs_.role() != GS::Role::Local) { o["error"] = "only the host starts the match decision (role " + String(GS::to_string(gs_.role())) + ")"; return o; }
	gs_.set_local_mods(gs_build_manifest());
	const auto &dis = gs_.lock_match();
	Dictionary disabled = gs_disabled_dict();
	Array active;
	for (auto &kv : gs_.local_mods()) if (!dis.count(kv.first)) active.push_back(G(kv.first));
	if (!dis.empty()) {
		String l;
		for (auto &kv : dis) {
			String who;
			for (size_t i = 0; i < kv.second.size(); i++) {
				const String id = G(kv.second[i].player);
				who += (i ? ", " : "") + (gs_peer_names_.count(id) ? gs_peer_names_[id] : id) + (kv.second[i].why == "missing" ? String() : String(" (") + G(kv.second[i].why) + ")");
			}
			l += (l.is_empty() ? "" : "; ") + G(kv.first) + " - missing for " + who;
		}
		gs_notice("off for everyone this match (a player does not have it): " + l + ".");
	}
	jvm::log("api", "gameplay settings: match start: " + String::num_int64(active.size()) + " gameplay mod(s) on, " + String::num_int64(int64_t(dis.size())) + " off for everyone; settings locked until the match ends");
	Dictionary ev;
	ev["player"] = "match"; ev["name"] = "match"; ev["disabled"] = disabled; ev["active"] = active; ev["locked"] = true;
	emit_event("gameplay_mods_status", ev);
	gs_dirty_ = true;
	gs_refresh();
	o = ev.duplicate(); o["ok"] = true; o["error"] = "";
	return o;
}

bool JVModAPI::gameplay_match_end(const String &mod) {
	if (!use(mod, kSyncCap, false)) return false;
	if (!gs_.match_locked()) return false;
	gs_.unlock_match();
	jvm::log("api", "gameplay settings: match end: settings unlocked, every gameplay mod back on");
	gs_dirty_ = true;
	gs_refresh();
	return true;
}

Dictionary JVModAPI::gameplay_set_host_manifest(const String &mod, const Variant &payload) {
	Dictionary o;
	o["ok"] = false;
	if (!use(mod, kSyncCap)) { o["error"] = why_not(mod, kSyncCap); return o; }
	gs_update_role();
	if (gs_.role() != GS::Role::Client) { o["error"] = "only clients take the host's mod list (role " + String(GS::to_string(gs_.role())) + ")"; return o; }
	std::string text, why;
	GS::Manifest m; bool lo = false; std::set<std::string> off;
	if (!payload_text(payload, text) || !GS::decode_manifest(text, m, lo, &why, &off)) { o["error"] = "bad manifest: " + G(why.empty() ? std::string("expected bytes or text") : why); return o; }
	gs_.set_local_mods(gs_build_manifest());
	const bool changed = !gs_.host_mods_known() || gs_.host_mods() != m || gs_.host_disabled() != off;
	gs_.set_host_mods(m, off);
	gs_dirty_ = true;

	Array mine;
	for (auto &d : off) {
		std::string w;
		auto h = m.find(d);
		if (h != m.end() && !GS::mod_active(d, h->second, gs_.local_mods(), &w)) mine.push_back(G(d) + (w == "missing" ? String() : String(" (") + G(w) + ")"));
	}
	if (changed && !mine.is_empty()) {
		String l;
		for (int i = 0; i < mine.size(); i++) l += (i ? ", " : "") + String(mine[i]);
		gs_notice("you do not have " + l + ": the host turned it off for everyone this match.");
	}
	Dictionary disabled = gs_disabled_dict();
	Array active;
	for (auto &kv : gs_.local_mods()) if (gs_.mod_active_for(kv.first)) active.push_back(G(kv.first));
	if (changed && !disabled.is_empty()) {
		String l;
		Array k = disabled.keys();
		for (int i = 0; i < k.size(); i++) l += (i ? ", " : "") + String(k[i]);
		jvm::log("api", "gameplay settings: off for this match (stock for everyone): " + l);
	}
	Dictionary ev;
	ev["player"] = "local"; ev["name"] = "local"; ev["disabled"] = disabled; ev["active"] = active; ev["missing_here"] = mine; ev["locked"] = !off.empty();
	emit_event("gameplay_mods_status", ev);
	gs_refresh();
	o = ev.duplicate(); o["ok"] = true; o["error"] = ""; o["host_loader_ok"] = lo;
	return o;
}

bool JVModAPI::gameplay_mod_active_for(const String &mod, const String &mod_id, const String &player_id) {
	if (!use(mod, kCap, false)) return false;
	gs_update_role();
	gs_.set_local_mods(gs_build_manifest());
	return gs_.mod_active_for(S(mod_id), S(player_id));
}

Dictionary JVModAPI::gameplay_match_status(const String &mod) {
	Dictionary o;
	if (!use(mod, kCap, false)) return o;
	gs_update_role();
	gs_.set_local_mods(gs_build_manifest());
	o["role"] = GS::to_string(gs_.role());
	o["locked"] = gs_.role() == GS::Role::Client ? gs_.host_mods_known() : gs_.match_locked();
	o["disabled"] = gs_disabled_dict();
	Array active;
	for (auto &kv : gs_.local_mods()) if (gs_.mod_active_for(kv.first)) active.push_back(G(kv.first));
	o["active"] = active;
	if (gs_.role() == GS::Role::Local && !gs_.match_locked()) {
		Dictionary pv;
		for (auto &kv : gs_.preview_disabled()) {
			Array a;
			for (auto &w : kv.second) a.push_back(gs_peer_names_.count(G(w.player)) ? gs_peer_names_[G(w.player)] : G(w.player));
			pv[G(kv.first)] = a;
		}
		o["would_disable"] = pv;
	}
	return o;
}

Dictionary JVModAPI::gameplay_player_mods(const String &mod) {
	Dictionary o;
	if (!use(mod, kCap, false)) return o;
	gs_update_role();
	gs_.set_local_mods(gs_build_manifest());
	Dictionary me, mods, inactive;
	Array active;
	for (auto &kv : gs_.local_mods()) {
		mods[G(kv.first)] = G(kv.second);
		std::string why;
		if (gs_.mod_active_for(kv.first, "", &why)) active.push_back(G(kv.first));
		else inactive[G(kv.first)] = G(why);
	}
	me["name"] = "local"; me["mods"] = mods; me["loader_ok"] = gs_loader_ok(); me["active"] = active; me["inactive"] = inactive;
	o["local"] = me;
	if (gs_.role() == GS::Role::Client && gs_.host_mods_known()) {
		Dictionary h, list;
		for (auto &kv : gs_.host_mods()) list[G(kv.first)] = G(kv.second);
		Array off;
		for (auto &m : gs_.host_disabled()) off.push_back(G(m));
		h["mods"] = list; h["name"] = "host"; h["disabled"] = off;
		o["host"] = h;
	}
	for (auto &kv : gs_.peers()) {
		const String id = G(kv.first);
		Dictionary e, list;
		for (auto &m : kv.second.mods) list[G(m.first)] = G(m.second);
		e["name"] = gs_peer_names_.count(id) ? gs_peer_names_[id] : id;
		e["mods"] = list; e["loader_ok"] = kv.second.loader_ok; e["missing"] = gs_missing_of(kv.second.mods); e["late"] = gs_.is_late(kv.first);
		o[id] = e;
	}
	return o;
}
