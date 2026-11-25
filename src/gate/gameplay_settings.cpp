#include "gameplay_settings.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>

namespace jvm::gs {

const char *to_string(Type t) {
	switch (t) {
		case Type::Bool: return "bool";
		case Type::Int: return "int";
		case Type::Float: return "float";
		case Type::Enum: return "enum";
		case Type::Vec: return "vec";
	}
	return "?";
}
bool parse_type(const std::string &s, Type &out) {
	static const std::pair<const char *, Type> t[] = { { "bool", Type::Bool }, { "int", Type::Int }, { "float", Type::Float }, { "enum", Type::Enum }, { "vec", Type::Vec } };
	for (auto &p : t) if (s == p.first) { out = p.second; return true; }
	return false;
}
const char *to_string(Role r) {
	switch (r) { case Role::Off: return "off"; case Role::Local: return "local"; case Role::Client: return "client"; }
	return "?";
}

std::string check_id(const std::string &mod, const std::string &id) {
	if (mod.empty()) return "mod name is empty";
	if (id.size() > 96) return "id longer than 96 characters";
	if (id.rfind(mod + ".", 0) != 0 || id.size() <= mod.size() + 1) return "id must be namespaced '" + mod + ".<setting>' (got '" + id + "')";
	for (char c : id) {
		bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
		if (!ok) return "id may only use a-z 0-9 _ . - (got '" + id + "')";
	}
	if (id.back() == '.' || id.find("..") != std::string::npos) return "id has an empty segment ('" + id + "')";
	return "";
}

std::string check_sync_class(const std::string &sync) {
	if (sync == "gameplay") return "";
	if (sync == "visual" || sync == "cosmetic" || sync == "visual_only")
		return "visual-only settings are not gameplay settings: cosmetics stay per-player (use the mod's own config / the cosmetic APIs)";
	return "sync class must be \"gameplay\" (got \"" + sync + "\")";
}

static bool finite_all(const Value &v) { for (double x : v) if (!std::isfinite(x)) return false; return true; }

std::string check_def(Def &d) {
	switch (d.type) {
		case Type::Bool: d.dim = 1; d.min = 0; d.max = 1; d.step = 1; d.options.clear(); break;
		case Type::Enum:
			if (d.options.size() < 2) return "enum needs at least 2 options";
			if (d.options.size() > 64) return "enum has more than 64 options";
			{ std::set<std::string> seen; for (auto &o : d.options) { if (o.empty()) return "enum option is empty"; if (!seen.insert(o).second) return "enum option '" + o + "' repeated"; } }
			d.dim = 1; d.min = 0; d.max = double(d.options.size() - 1); d.step = 1; break;
		case Type::Int:
			d.dim = 1; d.options.clear();
			d.min = std::ceil(d.min); d.max = std::floor(d.max);
			d.step = d.step < 1 ? 1 : std::round(d.step); break;
		case Type::Float: d.dim = 1; d.options.clear(); if (d.step < 0) return "step must be >= 0"; break;
		case Type::Vec:
			if (d.dim < 2 || d.dim > 4) return "vec needs 2..4 components";
			d.options.clear(); if (d.step < 0) return "step must be >= 0"; break;
	}
	if (!std::isfinite(d.min) || !std::isfinite(d.max) || !std::isfinite(d.step)) return "min / max / step must be finite";
	if (d.min > d.max) return "min is above max";
	if (int(d.def.size()) == 1 && d.dim > 1) d.def.assign(size_t(d.dim), d.def[0]);
	if (int(d.def.size()) != d.dim) return "default needs " + std::to_string(d.dim) + " value(s)";
	if (!finite_all(d.def)) return "default must be finite";
	for (double x : d.def) if (x < d.min || x > d.max) return "default is outside min..max";
	if (d.label.empty()) d.label = d.id;
	if (d.category.empty()) d.category = d.mod;
	if (d.label.size() > 80 || d.tooltip.size() > 400 || d.category.size() > 40) return "label / tooltip / category too long (80 / 400 / 40)";
	return "";
}

bool sanitize(const Def &d, const double *in, int n, Value &out, bool *clamped) {
	if (clamped) *clamped = false;
	out.clear();
	int want = d.dim;
	if (n == 1 && want > 1) n = 1;
	else if (n != want) return false;
	for (int i = 0; i < n; i++) if (!std::isfinite(in[i])) return false;
	for (int i = 0; i < want; i++) {
		double v = in[n == 1 ? 0 : i];
		double c = v < d.min ? d.min : (v > d.max ? d.max : v);
		if (d.type == Type::Bool) c = c >= 0.5 ? 1.0 : 0.0;
		else if (d.type == Type::Int || d.type == Type::Enum) c = std::round(c);
		if (d.step > 0 && d.type != Type::Bool && d.type != Type::Enum) {
			c = d.min + std::round((c - d.min) / d.step) * d.step;
			if (c > d.max) c = d.max;
			if (c < d.min) c = d.min;
			if (d.type == Type::Int) c = std::round(c);

			c = std::round(c * 1e9) / 1e9;
		}
		if (clamped && std::fabs(c - v) > 1e-9 * (1.0 + std::fabs(v)) && (v < d.min || v > d.max)) *clamped = true;
		out.push_back(c);
	}
	return true;
}

Result Registry::add(Def d, const std::string &sync_class) {
	Result r;
	if ((r.error = check_sync_class(sync_class)) != "") return r;
	if ((r.error = check_id(d.mod, d.id)) != "") return r;
	if ((r.error = check_def(d)) != "") { r.error = d.id + ": " + r.error; return r; }
	auto it = defs_.find(d.id);
	if (it != defs_.end() && it->second.mod != d.mod) { r.error = d.id + " is registered by " + it->second.mod; return r; }
	if (it == defs_.end() && int(defs_.size()) >= kMaxSettings) { r.error = "too many settings"; return r; }
	if (it == defs_.end()) { d.order = int(order_.size()); order_.push_back(d.id); }
	else d.order = it->second.order;
	defs_[d.id] = d;

	auto lv = local_.find(d.id);
	if (lv != local_.end()) { Value s; if (sanitize(d, lv->second, s)) lv->second = s; else local_.erase(lv); }
	auto hv = host_.find(d.id);
	if (hv != host_.end()) { Value s; if (sanitize(d, hv->second, s)) hv->second = s; else host_.erase(hv); }
	revision_++;
	r.ok = true;
	return r;
}

int Registry::remove_mod(const std::string &mod) {
	int n = 0;
	for (auto it = defs_.begin(); it != defs_.end();) {
		if (it->second.mod == mod) { local_.erase(it->first); host_.erase(it->first); it = defs_.erase(it); n++; }
		else ++it;
	}
	if (n) {
		std::vector<std::string> keep;
		for (auto &id : order_) if (defs_.count(id)) keep.push_back(id);
		order_ = keep;
		int i = 0; for (auto &id : order_) defs_[id].order = i++;
		revision_++;
	}
	return n;
}

const Def *Registry::find(const std::string &id) const {
	auto it = defs_.find(id);
	return it == defs_.end() ? nullptr : &it->second;
}

std::vector<std::string> Registry::mods() const {
	std::vector<std::string> out;
	std::set<std::string> seen;
	for (auto &id : order_) { const std::string &m = defs_.at(id).mod; if (seen.insert(m).second) out.push_back(m); }
	return out;
}

Result Registry::set_local(const std::string &id, const Value &raw) {
	Result r;
	const Def *d = find(id);
	if (!d) { r.error = "unknown setting " + id; return r; }
	Value v;
	if (!sanitize(*d, raw, v)) { r.error = id + ": needs " + std::to_string(d->dim) + " finite value(s)"; return r; }
	if (v == d->def) local_.erase(id); else local_[id] = v;
	r.ok = true;
	return r;
}
bool Registry::reset_local(const std::string &id) { return local_.erase(id) != 0; }
const Value *Registry::local(const std::string &id) const {
	auto it = local_.find(id);
	return it == local_.end() ? nullptr : &it->second;
}

Value Registry::get(const std::string &id, std::string *source) const {
	const Def *d = find(id);
	if (!d) { if (source) *source = "unknown"; return Value(); }
	const Value *pick = nullptr;
	const char *src = "default";
	if (role_ == Role::Client) {

		auto hm = host_mods_.find(d->mod);
		auto lm = local_mods_.find(d->mod);
		bool host_has = !host_disabled_.count(d->mod) &&
				(!host_mods_known_ || (hm != host_mods_.end() && (lm == local_mods_.end() || version_major(hm->second) == version_major(lm->second))));
		auto h = host_.find(id);
		if (host_has && h != host_.end()) { pick = &h->second; src = "host"; } else src = "stock";
	}
	else if (role_ == Role::Local) {
		if (disabled_.count(d->mod)) src = "stock";
		else { auto l = local_.find(id); if (l != local_.end()) { pick = &l->second; src = "local"; } }
	}
	else src = "stock";
	if (source) *source = src;
	Value out;
	if (!pick || !sanitize(*d, *pick, out)) return d->def;
	return out;
}

std::map<std::string, Value> Registry::snapshot() const {
	std::map<std::string, Value> out;
	if (role_ != Role::Local) return out;
	for (auto &id : order_) if (!disabled_.count(defs_.at(id).mod)) out[id] = get(id);
	return out;
}

static std::string num(double v) {
	char b[40];
	std::snprintf(b, sizeof b, "%.17g", v);
	return b;
}
std::string Registry::encode(const std::map<std::string, Value> &values) {
	std::string s = "JVGS " + std::to_string(kSnapshotVersion) + "\n";
	for (auto &kv : values) {
		s += kv.first + "=";
		for (size_t i = 0; i < kv.second.size(); i++) { if (i) s += ","; s += num(kv.second[i]); }
		s += "\n";
	}
	return s;
}
uint64_t Registry::hash(const std::string &b) {
	uint64_t h = 1469598103934665603ull;
	for (unsigned char c : b) { h ^= c; h *= 1099511628211ull; }
	return h;
}

bool decode(const std::string &bytes, std::map<std::string, Value> &out, std::string *why) {
	auto no = [&](const std::string &w) { if (why) *why = w; out.clear(); return false; };
	out.clear();
	if (bytes.size() > size_t(kSnapshotMaxBytes)) return no("payload larger than 32 KiB");
	std::istringstream in(bytes);
	std::string line;
	if (!std::getline(in, line) || line.rfind("JVGS ", 0) != 0) return no("not a JVGS payload");
	if (line.substr(5) != std::to_string(kSnapshotVersion)) return no("unsupported payload version " + line.substr(5));
	while (std::getline(in, line)) {
		if (line.empty()) continue;
		if (int(out.size()) >= kSnapshotMaxEntries) return no("too many entries");
		size_t eq = line.find('=');
		if (eq == std::string::npos || eq == 0) return no("malformed line");
		std::string id = line.substr(0, eq);
		Value v;
		std::string rest = line.substr(eq + 1);
		size_t p = 0;
		while (p <= rest.size()) {
			size_t c = rest.find(',', p);
			std::string tok = rest.substr(p, c == std::string::npos ? std::string::npos : c - p);
			if (tok.empty() || tok.size() > 32) return no("malformed number in " + id);
			char *end = nullptr;
			double x = std::strtod(tok.c_str(), &end);
			if (!end || *end != '\0') return no("malformed number in " + id);
			v.push_back(x);
			if (v.size() > 4) return no("too many components in " + id);
			if (c == std::string::npos) break;
			p = c + 1;
		}
		out[id] = v;
	}
	return true;
}

ApplyResult Registry::apply_host_values(const std::map<std::string, Value> &raw) {
	ApplyResult r;
	r.ok = true;
	std::map<std::string, Value> next;
	for (auto &kv : raw) {
		const Def *d = find(kv.first);
		if (!d) { r.unknown++; continue; }
		Value v;
		if (!sanitize(*d, kv.second, v)) { r.rejected++; r.errors.push_back(kv.first + ": invalid value"); continue; }
		next[kv.first] = v;
		r.applied++;
	}
	host_ = next;
	return r;
}

ApplyResult Registry::apply_host(const std::string &bytes) {
	std::map<std::string, Value> raw;
	std::string why;
	if (!decode(bytes, raw, &why)) { ApplyResult r; r.errors.push_back(why); return r; }
	return apply_host_values(raw);
}

}

namespace jvm::gs {

std::string version_major(const std::string &v) {
	size_t i = 0;
	while (i < v.size() && v[i] == ' ') i++;
	size_t j = i;
	while (j < v.size() && v[j] != '.') j++;
	return v.substr(i, j - i);
}

static bool manifest_name_ok(const std::string &s) {
	if (s.empty() || s.size() > 64) return false;
	for (char c : s) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
	return true;
}
static bool manifest_version_ok(const std::string &s) {
	if (s.empty() || s.size() > 32) return false;
	for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || c == '.' || c == '-' || c == '_')) return false;
	return true;
}

std::string encode_manifest(const Manifest &mods, bool loader_ok, const std::set<std::string> *disabled) {
	std::string o = "JVGM " + std::to_string(kManifestVersion) + "\nloader=" + (loader_ok ? "1" : "0") + "\n";
	for (auto &kv : mods) if (manifest_name_ok(kv.first) && manifest_version_ok(kv.second)) o += kv.first + "=" + kv.second + "\n";
	if (disabled) for (auto &m : *disabled) if (manifest_name_ok(m)) o += "!" + m + "=1\n";
	return o;
}

bool decode_manifest(const std::string &bytes, Manifest &mods, bool &loader_ok, std::string *why, std::set<std::string> *disabled) {
	auto fail = [&](const char *w) { if (why) *why = w; return false; };
	if (bytes.size() > size_t(kManifestMaxBytes)) return fail("manifest too large");
	if (bytes.compare(0, 5, "JVGM ") != 0) return fail("not a JVGM manifest");
	size_t nl = bytes.find('\n');
	if (nl == std::string::npos || bytes.substr(5, nl - 5) != std::to_string(kManifestVersion)) return fail("unsupported manifest version");
	Manifest out;
	std::set<std::string> off;
	bool loader = false, saw_loader = false;
	size_t pos = nl + 1;
	while (pos < bytes.size()) {
		size_t e = bytes.find('\n', pos);
		if (e == std::string::npos) e = bytes.size();
		std::string line = bytes.substr(pos, e - pos);
		pos = e + 1;
		if (line.empty()) continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos) return fail("bad manifest line");
		std::string k = line.substr(0, eq), v = line.substr(eq + 1);
		if (k == "loader") { loader = v == "1"; saw_loader = true; continue; }
		if (!k.empty() && k[0] == '!') {
			if (!manifest_name_ok(k.substr(1))) return fail("bad mod name");
			off.insert(k.substr(1));
			continue;
		}
		if (!manifest_name_ok(k) || !manifest_version_ok(v)) return fail("bad mod name or version");
		if (out.size() >= size_t(kManifestMaxMods)) return fail("too many mods");
		out[k] = v;
	}
	mods = out;
	if (disabled) *disabled = off;
	loader_ok = saw_loader && loader;
	return true;
}

bool mod_active(const std::string &mod, const std::string &host_version, const Manifest &player, std::string *why) {
	auto it = player.find(mod);
	if (it == player.end()) { if (why) *why = "missing"; return false; }
	if (version_major(it->second) != version_major(host_version)) {
		if (why) *why = "version " + it->second + " (host " + host_version + ")";
		return false;
	}
	if (why) why->clear();
	return true;
}

std::map<std::string, std::vector<Missing>> Registry::preview_disabled() const {
	std::map<std::string, std::vector<Missing>> out;
	for (auto &m : local_mods_)
		for (auto &p : peers_) {
			if (match_locked_ && !match_players_.count(p.first)) continue;
			std::string why;
			if (!mod_active(m.first, m.second, p.second.mods, &why)) out[m.first].push_back(Missing{ p.first, why });
		}
	return out;
}

const std::map<std::string, std::vector<Missing>> &Registry::lock_match() {
	match_locked_ = false;
	match_players_.clear();
	for (auto &p : peers_) match_players_.insert(p.first);
	disabled_ = preview_disabled();
	match_locked_ = true;
	return disabled_;
}

bool Registry::match_disabled(const std::string &mod) const {
	if (role_ == Role::Local) return disabled_.count(mod) > 0;
	if (role_ == Role::Client) {
		if (host_disabled_.count(mod)) return true;
		if (!host_mods_known_) return false;
		auto h = host_mods_.find(mod);
		auto l = local_mods_.find(mod);
		return h == host_mods_.end() || (l != local_mods_.end() && version_major(h->second) != version_major(l->second));
	}
	return false;
}

bool Registry::mod_active_for(const std::string &mod, const std::string &, std::string *why) const {
	auto set = [&](const std::string &w) { if (why) *why = w; };
	if (role_ == Role::Off) { set("gameplay closed (stock)"); return false; }
	auto mine = local_mods_.find(mod);
	if (mine == local_mods_.end()) { set("not installed"); return false; }
	if (role_ == Role::Local) {
		auto d = disabled_.find(mod);
		if (d == disabled_.end()) { set(""); return true; }
		std::string w = "off for this match: ";
		for (size_t i = 0; i < d->second.size(); i++) w += (i ? ", " : "") + d->second[i].player + " (" + d->second[i].why + ")";
		set(w);
		return false;
	}
	if (!host_mods_known_) { set("waiting for the host's mod list (stock)"); return false; }
	auto h = host_mods_.find(mod);
	if (h == host_mods_.end()) { set("off for this match: the host does not have it"); return false; }
	std::string w;
	if (!mod_active(mod, h->second, local_mods_, &w)) { set("off for this match: " + w); return false; }
	if (host_disabled_.count(mod)) { set("off for this match: a player does not have it"); return false; }
	set("");
	return true;
}

}
