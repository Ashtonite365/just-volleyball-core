#include "api_core.hpp"

#include <cstdlib>
#include <sstream>
#include <unordered_map>

namespace jvm::api {

std::string version_string() {
	return std::to_string(kVersionMajor) + "." + std::to_string(kVersionMinor) + "." + std::to_string(kVersionPatch);
}

#define READ Access::Read
#define LOCAL_UI Access::LocalUi
#define COSMETIC Access::Cosmetic
#define GAMEPLAY Access::Gameplay
#define SYNC Access::Sync
#define OFFLINE Access::Offline
#define LOCAL Scope::Local
#define HOST Scope::Host
#define PEERS Scope::Peers
#define IMPL Status::Impl
#define MAPPED Status::Mapped
#define PLANNED Status::Planned
static const std::vector<Capability> kCaps = {
#define JV_CAP(id, since, acc, scope, status, feature, refs, fns) { id, since, acc, scope, status, feature, refs, fns },
#include "capabilities.def"
#undef JV_CAP
};
#undef READ
#undef LOCAL_UI
#undef COSMETIC
#undef GAMEPLAY
#undef SYNC
#undef OFFLINE
#undef LOCAL
#undef HOST
#undef PEERS
#undef IMPL
#undef MAPPED
#undef PLANNED

const std::vector<Capability> &capabilities() { return kCaps; }



const Capability *find(const std::string &id) {
	static const std::unordered_map<std::string, const Capability *> index = [] {
		std::unordered_map<std::string, const Capability *> m;
		for (auto &c : kCaps) m.emplace(c.id, &c);
		return m;
	}();
	auto it = index.find(id);
	return it == index.end() ? nullptr : it->second;
}

const char *to_string(Access a) {
	switch (a) {
		case Access::Read: return "read";
		case Access::LocalUi: return "local_ui";
		case Access::Cosmetic: return "cosmetic";
		case Access::Gameplay: return "gameplay";
		case Access::Sync: return "sync";
		case Access::Offline: return "offline";
	}
	return "?";
}
const char *to_string(Scope s) {
	switch (s) { case Scope::Local: return "local"; case Scope::Host: return "host"; case Scope::Peers: return "peers"; }
	return "?";
}
const char *to_string(Status s) {
	switch (s) { case Status::Impl: return "impl"; case Status::Mapped: return "mapped"; case Status::Planned: return "planned"; }
	return "?";
}

bool is_sync_mode(const std::string &m) { return m == "LAN" || m == "private" || m == "friends-only"; }
bool is_offline_mode(const std::string &m) { return m == "Free Play" || m == "practice" || m == "offline solo" || m == "menu"; }

bool access_open(Access a, const GateView &g, std::string *why) {
	auto no = [&](const std::string &w) { if (why) *why = w; return false; };
	if (!g.pin_ok) return no("game version not pinned (mods off)");
	switch (a) {
		case Access::Read:
		case Access::LocalUi:
			return true;
		case Access::Cosmetic:
			return g.cosmetic ? true : no(g.cosmetic_reason.empty() ? "cosmetic tier closed (" + g.mode + ")" : g.cosmetic_reason);
		case Access::Gameplay:
			return g.full ? true : no(g.full_reason.empty() ? "gameplay closed in " + g.mode : g.full_reason);
		case Access::Sync:
			if (!g.full) return no(g.full_reason.empty() ? "gameplay closed in " + g.mode : g.full_reason);
			return is_sync_mode(g.mode) ? true : no("sync only in LAN / private / friends-only lobbies (mode: " + g.mode + ")");
		case Access::Offline:
			if (!g.full) return no(g.full_reason.empty() ? "gameplay closed in " + g.mode : g.full_reason);
			return is_offline_mode(g.mode) ? true : no("offline only: Free Play / practice / offline solo / menu (mode: " + g.mode + ")");
	}
	return no("unknown access class");
}

static bool parse_version(const std::string &s, int &maj, int &min) {
	maj = min = 0;
	if (s.empty()) return false;
	std::stringstream ss(s);
	std::string part;
	int idx = 0;
	while (std::getline(ss, part, '.')) {
		if (part.empty() || part.find_first_not_of("0123456789") != std::string::npos || part.size() > 6) return false;
		int v = std::atoi(part.c_str());
		if (idx == 0) maj = v; else if (idx == 1) min = v;
		if (++idx > 3) return false;
	}
	return idx >= 1;
}

bool version_compatible(const std::string &wanted, std::string *why) {
	int maj, min;
	if (!parse_version(wanted, maj, min)) { if (why) *why = "bad API version '" + wanted + "'"; return false; }
	if (maj != kVersionMajor) { if (why) *why = "mod wants API " + wanted + ", this is " + version_string() + " (major differs)"; return false; }
	if (min > kVersionMinor) { if (why) *why = "mod wants API " + wanted + ", this is " + version_string() + " (too old)"; return false; }
	return true;
}

RegisterResult Registry::register_mod(const std::string &name, const std::string &wanted, const std::vector<std::string> &caps) {
	RegisterResult r;
	if (name.empty() || name.size() > 64) { r.reason = "bad mod name"; return r; }
	std::string why;
	if (!version_compatible(wanted, &why)) { r.reason = why; return r; }
	Mod &m = mods_[name];
	m.name = name;
	m.wanted_version = wanted;
	m.declared.clear();
	for (auto &id : caps) {
		const Capability *c = find(id);
		if (!c) { r.denied.push_back(id + ": unknown capability"); continue; }
		if (c->status == Status::Planned) { r.denied.push_back(id + ": planned, not available yet"); continue; }
		int cmaj, cmin, wmaj, wmin;
		parse_version(c->since, cmaj, cmin);
		parse_version(wanted, wmaj, wmin);
		if (cmaj > wmaj || (cmaj == wmaj && cmin > wmin)) { r.denied.push_back(id + ": needs API " + c->since); continue; }
		m.declared.insert(id);
		r.granted.push_back(id);
	}
	r.ok = true;
	r.reason = r.denied.empty() ? "registered" : "registered (some capabilities refused)";
	return r;
}

CheckResult Registry::peek(const std::string &mod, const std::string &cap, const GateView &g, bool need_open) const {
	CheckResult r;
	const Capability *c = find(cap);
	if (!c) { r.why = "unknown capability " + cap; return r; }
	if (c->status == Status::Planned) { r.why = cap + " is planned, not available yet"; return r; }
	auto it = mods_.find(mod);
	if (it == mods_.end()) { r.why = "mod '" + mod + "' is not registered with the API"; return r; }
	if (!it->second.declared.count(cap)) { r.why = "mod '" + mod + "' did not declare " + cap; return r; }
	std::string why;
	if (need_open && !access_open(c->access, g, &why)) { r.why = cap + " (" + to_string(c->access) + "): " + why; return r; }
	r.ok = true;
	return r;
}

CheckResult Registry::check(const std::string &mod, const std::string &cap, const GateView &g, bool need_open) {
	CheckResult r = peek(mod, cap, g, need_open);
	auto it = mods_.find(mod);
	if (it != mods_.end()) (r.ok ? it->second.calls : it->second.refusals)[cap]++;
	return r;
}


const std::vector<GoofyParam> &goofy_params() {
#define PHYS "PHYS"
#define FEEL "FEEL"
#define EXT "EXT"
#define GP(id, sec, tgt, prop, col, a0, a1, a2, a3, mn, mx, unit, apply) GoofyParam{ id, sec, tgt, prop, col, { a0, a1, a2, a3 }, mn, mx, unit, apply },
	static const std::vector<GoofyParam> t = {
#include "../gate/goofy_params.inc"
#include "../gate/goofy_player_params.inc"
	};
#undef GP
#undef PHYS
#undef FEEL
#undef EXT
	return t;
}
int goofy_ball_param_count() {
	static const int n = [] { int c = 0;
#define GP(...) c++;
#include "../gate/goofy_params.inc"
#undef GP
		return c; }();
	return n;
}
bool goofy_is_player_library(const GoofyParam &p) {
	const auto &t = goofy_params();
	return &p >= t.data() + goofy_ball_param_count() && &p < t.data() + t.size();
}
const GoofyParam *find_goofy_param(const std::string &id) {
	for (auto &p : goofy_params()) if (id == p.id) return &p;
	return nullptr;
}

const std::vector<EventInfo> &events() {
	static const std::vector<EventInfo> e = {
		{ "access_changed", "gate.mode", "{access, open, reason}", "an access class opens or closes (same as signal access_changed)" },
		{ "mode_changed", "gate.mode", "{mode, previous}", "the detected play mode changes (Free Play -> LAN, menu -> public, ...)" },
		{ "scene_changed", "scene.current", "{scene, name, previous_name}", "the running scene changes (menu <-> match <-> practice)" },
		{ "stage_picked", "stage.pick", "{id, previous}", "the picked custom court changes (\"\" = a stock stage)" },
		{ "ball_picked", "ball.pick", "{id, previous}", "the picked custom ball changes (\"\" = stock)" },
		{ "loader_map_changed", "loader.query", "{id, previous}", "the unified loader's current map changes (\"\" = stock map)" },
		{ "model_changed", "models.query", "{id, previous}", "the shown player model changes (saved pick or preview)" },
		{ "menu_perf_changed", "menu.perf", "{enabled}", "the menu perf toggle changes" },
		{ "goofy_params_changed", "goofy.query", "{changed, effective}", "Goofy's effective ball params change (relayed from Goofy API 1.0 params_changed)" },
		{ "goofy_ball_mode_changed", "goofy.query", "{mode, force_defaults}", "Ball override NORMAL/FIXED changes (relayed from ball_mode_changed)" },
		{ "goofy_physics_allowed_changed", "goofy.query", "{allowed}", "Goofy physics become allowed/blocked (relayed from physics_allowed_changed)" },
		{ "goofy_ball_event", "goofy.events", "{name, index, args}", "a ball signal fired: bounced, struck(pos, power, kind), net_hit, wall_hit, antenna_hit(side); index = goofy_balls() index" },
		{ "goofy_player_event", "goofy.events", "{name, index, args}", "a player signal fired: touched, swung, thumped, knock_started, touch_attempted, out_of_range, launched, jumped, callout_made, high_fived, block_broken, dove_into_net(_pole); index = goofy_players() index" },
		{ "goofy_sync_changed", "goofy.query", "{role, host_values}", "Goofy host sync role (off/offline/host/client) or the host's values changed" },
		{ "gameplay_setting_changed", "gameplay.settings", "{id, value, previous, source}", "a registered gameplay setting's EFFECTIVE value changed (local edit, host values arrived, or the role changed: offline / host / client / off)" },
		{ "gameplay_role_changed", "gameplay.settings", "{role, previous}", "the registry's role changed: off (stock), local (offline / host), client (host's values)" },
		{ "gameplay_mods_status", "gameplay.settings", "{player, name, disabled, active, missing, late, locked, ...}", "the match-wide missing-mod rule ran: a player's mod list reached the host (player id, missing = host mods they lack, late = joined mid-match), the host decided at match start (player \"match\", disabled = mod -> who lacks it: off for everyone), or a client got the host's list (player \"local\")" },
	};
	return e;
}
const EventInfo *find_event(const std::string &name) {
	for (auto &e : events()) if (name == e.name) return &e;
	return nullptr;
}

const std::vector<std::string> &allowed_signals(const std::string &domain) {

	static const std::vector<std::string> ball = { "bounced", "struck", "net_hit", "wall_hit", "antenna_hit" };
	static const std::vector<std::string> player = { "touched", "swung", "thumped", "knock_started", "touch_attempted", "out_of_range", "launched", "jumped", "callout_made", "high_fived", "block_broken", "dove_into_net", "dove_into_net_pole" };
	static const std::vector<std::string> net = { "lobby_changed", "lobby_tod_changed", "session_ended", "match_started", "peer_left_match", "peer_match_ready", "late_joiner", "party_joined", "party_became_room", "party_ball", "party_stage", "party_look" };
	static const std::vector<std::string> none;
	if (domain == "ball") return ball;
	if (domain == "player") return player;
	if (domain == "net") return net;
	return none;
}
bool allowed_signal(const std::string &domain, const std::string &name) {
	for (auto &s : allowed_signals(domain)) if (s == name) return true;
	return false;
}

const char *library_path(const std::string &lib) {
	if (lib == "loader") return "/root/JVLoader";
	if (lib == "textures") return "/root/JVTextureLoader";
	if (lib == "models") return "/root/JVModelLoader";
	if (lib == "shots") return "/root/JVExtraShots";
	if (lib == "goofy") return "/root/JVGoofyMods";
	if (lib == "menu_ui") return "/root/JVModGate/JVMenuUI";
	if (lib == "menu_perf") return "/root/JVModGate/JVMenuPerf";
	if (lib == "picker") return "/root/JVModGate/JVMenuUI/JVPickerUI";
	return "";
}

const std::vector<std::string> &mode_labels() {
	static const std::vector<std::string> m = { "Free Play", "practice", "offline solo", "menu", "LAN", "private", "friends-only", "public", "quick match", "server browser", "unknown" };
	return m;
}

const std::set<std::string> &migrated_libraries() {


	static const std::set<std::string> s = { "goofy", "shots", "loader", "textures", "models" };
	return s;
}

bool library_allowed(const std::string &short_name, bool strict, std::string *why) {
	if (!strict) return true;
	if (migrated_libraries().count(short_name)) return true;
	if (why) *why = "strict API mode: " + short_name + " still calls the game directly (not migrated)";
	return false;
}

Coverage coverage(const Registry *r) {
	Coverage c;
	std::set<std::string> used;
	if (r) for (auto &kv : r->mods()) used.insert(kv.second.declared.begin(), kv.second.declared.end());
	for (auto &cap : kCaps) {
		c.total++;
		if (cap.status == Status::Impl) c.impl++;
		else if (cap.status == Status::Mapped) c.mapped++;
		else c.planned++;
		c.by_access[to_string(cap.access)]++;
		std::string refs = cap.game_refs;
		if (refs != "-") {
			c.game_refs++;
			for (char ch : refs) if (ch == ';') c.game_refs++;
		}
	}
	c.used_by_mods = int(used.size());
	return c;
}

}
