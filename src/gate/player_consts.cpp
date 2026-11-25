
#include "player_consts.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>

namespace jvm::pconsts {


static const std::vector<Spec> kSpecs = {
	{"COMBO_SPIKE_BONUS", 1.2, 0.0, 4.8, false},
	{"FLOAT_BOT_PACE", 0.55, 0.0, 2.2, false},
	{"SPRINT_CHARGE_GRACE", 0.5, 0.0, 2.0, false},
	{"SPIKE_TURN_FREE", 0.65, 0.0, 2.6, false},
	{"SPIKE_TURN_FULL", 2.6, 0.0, 10.4, false},
	{"SPIKE_TURN_FLOOR", 0.15, 0.0, 1.0, false},
	{"SPIKE_TURN_SLOW", 2.0, 0.0, 8.0, false},
	{"SPIKE_TURN_FAST", 7.0, 0.0, 28.0, false},
	{"SPIKE_TURN_SLOW_COST", 0.25, 0.0, 1.0, false},
	{"SPIKE_TURN_RATE_BLEND", 0.5, 0.0, 2.0, false},
	{"JUMP_ORPHAN_S", 0.2, 0.0, 1.0, false},
	{"JUMP_SPIKE_GRACE_US", 140000.0, 0.0, 560000.0, true},
	{"SPIKE_RECOVER", 0.75, 0.0, 3.0, false},
	{"BAM_NET_SLACK", 0.15, 0.0, 1.0, false},
	{"DIVE_HIT_SHARE", 0.5, 0.0, 2.0, false},
	{"IN_PLAY_DY", 2.0, 0.0, 8.0, false},
	{"SIDE_DIVE_DEG", 70.0, 0.0, 89.0, false},
	{"DIVE_NET_MARGIN", 0.9, 0.0, 3.6, false},
	{"DIVE_NET_STOP", 0.7, 0.0, 2.8, false},
	{"NET_POLE_X", 4.7, 0.0, 18.8, false},
	{"NET_POLE_REACH", 0.25, 0.0, 1.0, false},
	{"FRONT_ZONE_GRACE", 0.3, 0.0, 1.2, false},
	{"SET_DUMP_SHORT", 1.2, 0.0, 4.8, false},
	{"SET_DUMP_MIN_DEPTH", 0.6, 0.0, 2.4, false},
	{"SET_DUMP_RISE_TAP", 2.8, 0.0, 11.2, false},
	{"SET_DUMP_RISE_FULL", 0.2, 0.0, 1.0, false},
	{"SET_POCKET_DEPTH", 1.75, 0.0, 7.0, false},
	{"SET_SELF_ZONE", 1.0, 0.0, 4.0, false},
	{"SET_MIN_DEPTH", 0.35, 0.0, 1.4, false},
	{"SET_APRON", 1.0, 0.0, 4.0, false},
	{"BACK_SET_DOT", -0.35, -1.0, 1.0, false},
	{"BLOCK_TAPE_SLACK", 0.05, 0.0, 1.0, false},
	{"BLOCK_TURN", 2.0, 0.0, 8.0, false},


	{"SET_AUTO_LOOKAHEAD", 2.0, 0.0, 8.0, false},
};

const std::vector<Spec> &specs() { return kSpecs; }
const Spec *find(const std::string &name) {
	for (const Spec &s : kSpecs) if (name == s.name) return &s;
	return nullptr;
}

std::string manifest_id(const std::string &pin) { return "player.consts@" + pin; }

void Lock::warn(const std::string &w) { ++rejected_; warnings_.push_back(w); if (warnings_.size() > 64) warnings_.erase(warnings_.begin()); }

static std::string num(double v) { char b[64]; std::snprintf(b, sizeof b, "%.17g", v); return b; }

static bool sanitize(const Spec &s, double v, double &out) {
	if (!std::isfinite(v)) return false;
	v = std::min(std::max(v, s.lo), s.hi);
	if (s.is_int) v = std::round(v);
	out = v;
	return true;
}

Result Lock::set(const std::string &name, double v, const std::string &source) {
	Result r;
	if (phase_ == Phase::Match) {
		r.error = "player.consts is locked for this match: " + name + " not changed (" + source + "); edit it in the lobby";
		warn(r.error);
		return r;
	}
	const Spec *s = find(name);
	if (!s) { r.error = "unknown player const " + name; return r; }
	double c;
	if (!sanitize(*s, v, c)) { r.error = "value for " + name + " is not a finite number"; return r; }
	if (c == s->stock) saved_.erase(name); else saved_[name] = c;
	r.ok = true;
	return r;
}

Result Lock::reset(const std::string &name, const std::string &source) {
	const Spec *s = find(name);
	if (!s) return Result{false, "unknown player const " + name};
	return set(name, s->stock, source);
}

Plan Lock::begin_match_host(uint64_t match_id, Context ctx, const std::vector<std::string> &players, const ActiveFn &active) {
	phase_ = Phase::Match;
	match_id_ = match_id;
	plan_ = Plan();
	plan_.match_id = match_id;
	plan_applied_ = true;
	has_early_ = false;
	if (ctx == Context::Public) return plan_;
	if (ctx == Context::Client) { plan_applied_ = false; return plan_; }
	for (const auto &p : players) {
		std::string why;
		if (!(active && active(p, &why))) plan_.missing.push_back({p, why.empty() ? "missing" : why});
	}
	if (plan_.missing.empty()) {
		plan_.values = saved_;
		plan_.apply = true;
	} else if (!saved_.empty()) {
		std::string names;
		for (const auto &m : plan_.missing) names += (names.empty() ? "" : ", ") + m.first + " (" + m.second + ")";
		plan_.notice = "Player consts are OFF this match (everyone plays stock): " + names + " " +
				(plan_.missing.size() == 1 ? "doesn't" : "don't") + " have player.consts";
	}
	return plan_;
}

void Lock::begin_match_client(uint64_t match_id, double now_s) {
	if (has_early_ && now_s >= 0.0 && early_at_ >= 0.0 && now_s - early_at_ > kEarlyPlanMaxAgeS) {
		warn("player.consts: dropped a stale plan from the host (older than the match start window)");
		clear_pending();
	}
	phase_ = Phase::Match;
	match_id_ = match_id;
	plan_ = Plan();
	plan_.match_id = match_id;
	plan_applied_ = false;
	if (has_early_) { plan_ = early_; plan_applied_ = true; has_early_ = false; }
}

Result Lock::on_plan(const std::string &bytes, bool from_host, double now_s) {
	Result r;
	if (!from_host) { r.error = "player.consts plan not from the host: ignored"; warn(r.error); return r; }
	Plan p; std::string why;
	if (!decode_plan(bytes, p, &why)) { r.error = "player.consts plan rejected: " + why; warn(r.error); return r; }
	if (phase_ == Phase::Lobby) {
		early_ = p; has_early_ = true; early_at_ = now_s; r.ok = true; return r;
	}
	if (plan_applied_ && p.match_id != plan_.match_id) {
		early_ = p; has_early_ = true; early_at_ = now_s; r.ok = true;
		warn("player.consts: plan for the next match kept (this match stays frozen)");
		return r;
	}
	if (plan_applied_) {
		r.error = "player.consts is locked for this match: sync packet ignored";
		warn(r.error);
		return r;
	}
	plan_ = p; plan_applied_ = true; r.ok = true;
	return r;
}

std::map<std::string, double> Lock::values() const {
	std::map<std::string, double> out;
	for (const Spec &s : kSpecs) out[s.name] = s.stock;
	if (phase_ != Phase::Match || !plan_applied_ || !plan_.apply) return out;
	for (const auto &kv : plan_.values) out[kv.first] = kv.second;
	return out;
}

std::map<std::string, double> Lock::end_match() {
	phase_ = Phase::Lobby;
	plan_ = Plan();
	plan_applied_ = false;

	std::map<std::string, double> out;
	for (const Spec &s : kSpecs) out[s.name] = s.stock;
	return out;
}


static const size_t kMaxPlan = 16384;
std::string encode_plan(const Plan &p) {
	std::string s = "JVPC 1\nmatch=" + std::to_string(p.match_id) + "\napply=" + (p.apply ? "1" : "0") + "\n";
	if (p.apply) for (const auto &kv : p.values) s += kv.first + "=" + num(kv.second) + "\n";
	return s;
}

bool decode_plan(const std::string &bytes, Plan &out, std::string *why) {
	auto fail = [&](const char *m) { if (why) *why = m; return false; };
	if (bytes.size() > kMaxPlan) return fail("oversize");
	std::istringstream in(bytes);
	std::string line;
	if (!std::getline(in, line) || line != "JVPC 1") return fail("bad header");
	Plan p;
	if (!std::getline(in, line) || line.rfind("match=", 0) != 0) return fail("no match id");
	char *end = nullptr; p.match_id = std::strtoull(line.c_str() + 6, &end, 10);
	if (!end || *end) return fail("bad match id");
	if (!std::getline(in, line) || (line != "apply=0" && line != "apply=1")) return fail("no apply flag");
	p.apply = line == "apply=1";
	std::set<std::string> seen;
	while (std::getline(in, line)) {
		if (line.empty()) continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos) return fail("bad line");
		std::string k = line.substr(0, eq);
		const Spec *s = find(k);
		if (!s) return fail("unknown const");
		if (!seen.insert(k).second) return fail("duplicate const");
		char *e2 = nullptr; double v = std::strtod(line.c_str() + eq + 1, &e2);
		if (!e2 || *e2) return fail("not a number");
		double c; if (!sanitize(*s, v, c)) return fail("not finite");
		p.values[k] = c;
	}
	if (!p.apply && !p.values.empty()) return fail("values in a stock plan");
	out = p;
	return true;
}

}
