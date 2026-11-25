#include "const_check.hpp"

#include <cmath>
#include <cstdio>

namespace jvm {
namespace consts {

const std::vector<std::string> &gameplay_names() {
	static const std::vector<std::string> n = {
		"COMBO_SPIKE_BONUS", "FLOAT_BOT_PACE", "SPIKE_RECOVER",
		"SPRINT_CHARGE_GRACE", "JUMP_ORPHAN_S", "JUMP_SPIKE_GRACE_US", "BAM_NET_SLACK",
		"SPIKE_TURN_FREE", "SPIKE_TURN_FULL", "SPIKE_TURN_FLOOR", "SPIKE_TURN_SLOW", "SPIKE_TURN_FAST",
		"SPIKE_TURN_SLOW_COST", "SPIKE_TURN_RATE_BLEND",
		"DIVE_HIT_SHARE", "SIDE_DIVE_DEG", "DIVE_NET_MARGIN", "DIVE_NET_STOP", "NET_POLE_X", "NET_POLE_REACH", "IN_PLAY_DY",
		"FRONT_ZONE_GRACE", "SET_DUMP_SHORT", "SET_DUMP_MIN_DEPTH", "SET_DUMP_RISE_TAP",
		"SET_DUMP_RISE_FULL", "SET_POCKET_DEPTH", "SET_SELF_ZONE", "SET_MIN_DEPTH", "SET_APRON", "BACK_SET_DOT",
		"BLOCK_TAPE_SLACK", "BLOCK_TURN", "SET_AUTO_LOOKAHEAD",
	};
	return n;
}

const std::vector<std::string> &protocol_names() {
	static const std::vector<std::string> n = {
		"NET_FLOATS", "QUICK_CHAT", "QUICK_CHAT_ACTIONS", "CALL_VARIANTS", "CALLOUT_GESTURES", "CALL_SLACK_MS", "HIGH_FIVE_ON",
		"DUMP_PUSH_HOLD",
	};
	return n;
}



static bool is_visual(const std::string &n) {
	static const char *prefixes[] = { "POSE_", "CEL_HF_", "REACH_", "GESTURE_", "RECOIL_", "SFX_", "SND_", "COLOR_", "COL_", "ICON_", "BADGE_", "FLASH_", "RING_", "SHADOW_", "CALLOUT_BURST", "CALLOUT_COOLDOWN" };
	for (const char *p : prefixes) if (n.rfind(p, 0) == 0) return true;
	static const std::set<std::string> names = { "_MIN_ACTION_HOLD", "TOSS_CANCEL_SPEED", "HIGH_FIVE_HOLD", "DIVE_DENY_TIME", "RECOIL_DIR" };
	return names.count(n) > 0;
}

bool is_protocol(const std::string &n) {
	for (auto &p : protocol_names()) if (p == n) return true;

	if (n.rfind("CALLOUT_", 0) == 0 && n.rfind("CALLOUT_BURST", 0) != 0 && n.rfind("CALLOUT_COOLDOWN", 0) != 0) return true;
	return false;
}

Kind classify(const std::string &n) {
	for (auto &g : gameplay_names()) if (g == n) return Kind::Gameplay;
	if (is_protocol(n)) return Kind::Protocol;
	if (is_visual(n)) return Kind::Visual;
	return Kind::Unknown;
}

const char *kind_name(Kind k) {
	switch (k) {
		case Kind::Gameplay: return "gameplay";
		case Kind::Protocol: return "protocol";
		case Kind::Visual: return "visual";
		default: return "unknown";
	}
}

const std::vector<std::string> &folded_readers(const std::string &n) {
	static const std::vector<std::string> none;
	static const std::vector<std::string> net_floats = { "game/net/net.gd", "game/net/net_sync.gd", "game/replay/replay_data.gd", "game/ui/menu.gd", "game/ball/own_balls.gd" };
	static const std::vector<std::string> tape = { "debug hitbox view (hitbox_view.gd)" };
	if (n == "NET_FLOATS") return net_floats;
	if (n == "BLOCK_TAPE_SLACK") return tape;
	return none;
}

bool default_bounds(const Spec &s, double &lo, double &hi) {
	if (s.has_bounds) { lo = s.min; hi = s.max; return true; }
	if (!s.has_stock) return false;
	const double m = std::fabs(s.stock);
	if (s.stock > 0) { lo = 0.0; hi = std::fmax(m * 4.0, 1.0); }
	else if (s.stock < 0) { lo = -std::fmax(m * 4.0, 1.0); hi = 0.0; }
	else { lo = -1.0; hi = 1.0; }
	return true;
}

bool same(double a, double b) {
	return std::fabs(a - b) <= 1e-6 * std::fmax(1.0, std::fmax(std::fabs(a), std::fabs(b)));
}

static std::string num(double v) {
	char b[64];
	std::snprintf(b, sizeof b, "%.6g", v);
	return b;
}

static void add(std::vector<Finding> &out, Sev s, const char *code, const std::string &name, const std::string &detail, const std::string &src) {
	Finding f; f.sev = s; f.code = code; f.name = name; f.detail = detail; f.source = src;
	out.push_back(f);
}

std::vector<Finding> check_write(const Write &w, const Context &c, const Live &l) {
	std::vector<Finding> out;
	const Kind k = classify(w.name);
	if (k == Kind::Protocol) {
		std::string d = "never-change name (netcode / shared ids); writing it desyncs peers";
		auto &fr = folded_readers(w.name);
		if (!fr.empty()) { d += "; folded into"; for (auto &r : fr) d += " " + r; }
		add(out, Sev::Error, "PROTOCOL_NAME", w.name, d, w.source);
		return out;
	}
	if (k == Kind::Visual) { add(out, Sev::Error, "NOT_GAMEPLAY", w.name, "visual/animation/UI const: not bypassed, not a gameplay setting", w.source); return out; }
	if (k == Kind::Unknown) { add(out, Sev::Error, "UNKNOWN_NAME", w.name, "not one of the 34 gameplay player consts", w.source); return out; }

	if (!l.available) add(out, Sev::Error, "CAP_UNAVAILABLE", w.name, "player.consts unavailable (" + (l.reason.empty() ? std::string("bypass not mounted") : l.reason) + ")", w.source);
	else if (!c.cap_open) add(out, Sev::Error, "CAP_UNAVAILABLE", w.name, "gameplay class closed in mode '" + c.mode + "' (fail closed: stock values only)", w.source);
	if (l.available && (l.still_const.count(w.name) || !l.statics.count(w.name)))
		add(out, Sev::Error, "CONST_NOT_STATIC", w.name, "still a const in the loaded player script: the write would be ignored", w.source);
	if (c.match_active)
		add(out, Sev::Error, "MID_MATCH_WRITE", w.name, "player consts are frozen for the whole match; set them before match start (host distributes)", w.source);
	if (c.synced_lobby && !c.is_host)
		add(out, Sev::Error, "NON_HOST_WRITE", w.name, "only the host sets player consts in a synced lobby (" + c.mode + ")", w.source);
	if (!w.is_number) { add(out, Sev::Error, "BAD_TYPE", w.name, "value is not a number", w.source); return out; }
	if (!w.finite) { add(out, Sev::Error, "BAD_VALUE", w.name, "value is NaN or infinite", w.source); return out; }
	auto it = l.specs.find(w.name);
	Spec s = it != l.specs.end() ? it->second : Spec();
	if (w.name == "JUMP_SPIKE_GRACE_US") s.is_int = true;
	if (s.is_int && !w.is_integral) add(out, Sev::Error, "BAD_TYPE", w.name, "int const: value " + num(w.value) + " has a fraction", w.source);
	double lo, hi;
	if (default_bounds(s, lo, hi)) {
		if (w.value < lo - 1e-12 || w.value > hi + 1e-12)
			add(out, Sev::Error, "OUT_OF_RANGE", w.name, num(w.value) + " outside [" + num(lo) + ", " + num(hi) + "]" + (s.has_bounds ? "" : " (default clamp: sign kept, up to 4x stock)"), w.source);
	} else if (std::fabs(w.value) > 1e6) {
		add(out, Sev::Error, "OUT_OF_RANGE", w.name, num(w.value) + " beyond 1e6 (no stock known)", w.source);
	}
	return out;
}

std::vector<Finding> audit(const Live &l, const Context &c, const Expect &e) {
	std::vector<Finding> out;
	const std::string src = "audit";

	for (auto &[name, v] : l.statics) {
		if (is_protocol(name)) {
			std::string d = "never-change name was rewritten to a static var";
			auto &fr = folded_readers(name);
			if (!fr.empty()) { d += "; other scripts keep folded copies:"; for (auto &r : fr) d += " " + r; }
			add(out, Sev::Error, "PROTOCOL_REWRITTEN", name, d, src);
		} else if (classify(name) == Kind::Visual) {
			add(out, Sev::Warn, "VISUAL_REWRITTEN", name, "visual const rewritten (not part of the 34)", src);
		}
	}
	bool any_gameplay_static = false;
	for (auto &n : gameplay_names()) if (l.statics.count(n)) { any_gameplay_static = true; break; }
	if (!any_gameplay_static) return out;
	if (l.available) {
		for (auto &n : gameplay_names())
			if (!l.statics.count(n)) add(out, Sev::Warn, "CONST_NOT_REWRITTEN", n, "bypass active but this name is still const", src);
	}
	for (auto &n : gameplay_names()) {
		auto it = l.statics.find(n);
		if (it == l.statics.end()) continue;
		const double v = it->second;
		auto sp = l.specs.find(n);
		Spec s = sp != l.specs.end() ? sp->second : Spec();
		if (n == "JUMP_SPIKE_GRACE_US") s.is_int = true;
		const bool stock_known = s.has_stock;
		const bool is_stock = stock_known && same(v, s.stock);
		if (!std::isfinite(v)) { add(out, Sev::Error, "BAD_VALUE", n, "live value is NaN or infinite", src); continue; }
		double lo, hi;
		if (default_bounds(s, lo, hi) && (v < lo - 1e-12 || v > hi + 1e-12))
			add(out, Sev::Error, "OUT_OF_RANGE", n, "live " + num(v) + " outside [" + num(lo) + ", " + num(hi) + "]", src);
		if (s.is_int && std::fabs(v - std::round(v)) > 1e-9) add(out, Sev::Error, "BAD_TYPE", n, "int const holds " + num(v), src);

		if ((!c.cap_open || !l.available) && stock_known && !is_stock)
			add(out, Sev::Error, "NOT_STOCK_WHILE_CLOSED", n, "live " + num(v) + " != stock " + num(s.stock) + " while player.consts is closed (" + (l.available ? "mode " + c.mode : l.reason) + ")", src);

		if (c.match_active && e.start_valid) {
			auto st = e.start_set.find(n);
			if (st != e.start_set.end() && !same(v, st->second))
				add(out, Sev::Error, "MID_MATCH_CHANGE", n, "live " + num(v) + " != start-of-match " + num(st->second) + " (values are frozen for the match)", src);
		}

		if (!c.is_host && c.synced_lobby && e.host_valid) {
			auto hs = e.host_set.find(n);
			if (hs == e.host_set.end()) add(out, Sev::Error, "HOST_MISMATCH", n, "host set has no value for this name", src);
			else if (!same(v, hs->second)) add(out, Sev::Error, "HOST_MISMATCH", n, "client " + num(v) + " != host " + num(hs->second), src);
		}

		auto ap = e.approved.find(n);
		const bool baseline_known = ap != e.approved.end() || stock_known;
		const double baseline = ap != e.approved.end() ? ap->second : s.stock;
		if (baseline_known && !same(v, baseline) && !(c.match_active && e.start_valid && e.start_set.count(n) && same(v, e.start_set.at(n))) && !(e.host_valid && e.host_set.count(n) && same(v, e.host_set.at(n))))
			add(out, Sev::Error, "UNTRACKED_WRITE", n, "live " + num(v) + " was not set through the checked write path (expected " + num(baseline) + ")", src);

		if (stock_known && !is_stock) {
			auto &fr = folded_readers(n);
			if (!fr.empty()) {
				std::string d = "changed to " + num(v) + " but folded copies keep stock " + num(s.stock) + ":";
				for (auto &r : fr) d += " " + r;
				add(out, Sev::Warn, "FOLDED_COPY_STALE", n, d, src);
			}
		}
	}
	if (!c.is_host && c.synced_lobby && e.host_valid) {
		for (auto &[n, hv] : e.host_set) {
			if (classify(n) != Kind::Gameplay) add(out, Sev::Error, "HOST_MISMATCH", n, "host set carries a non-gameplay name", src);
			else if (!l.statics.count(n)) add(out, Sev::Error, "HOST_MISMATCH", n, "host sets it but it is still const here", src);
		}
	}
	return out;
}

bool has_error(const std::vector<Finding> &f) {
	for (auto &x : f) if (x.sev == Sev::Error) return true;
	return false;
}

}
}
