#include "gate_logic.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace jvm::gate {

static std::string lower(std::string s) {
	for (auto &c : s) c = (char)std::tolower((unsigned char)c);
	s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return c == ' ' || c == '-' || c == '_'; }), s.end());
	return s;
}

Context context_from_string(const std::string &in) {
	const std::string s = lower(in);
	if (s == "offline" || s == "menu" || s == "none") return Context::Offline;
	if (s == "lan" || s == "local") return Context::Lan;
	if (s == "customlobby" || s == "custom" || s == "lobby") return Context::CustomLobby;
	if (s == "quickmatch" || s == "matchmaking" || s == "quickplay") return Context::QuickMatch;
	if (s == "serverbrowser" || s == "browser") return Context::ServerBrowser;
	if (s == "online") return Context::Online;
	if (s == "offlineplay" || s == "freeplay" || s == "practice" || s == "offlinesolo" || s == "solo") return Context::OfflinePlay;
	return Context::Unknown;
}

Visibility visibility_from_string(const std::string &in) {
	const std::string s = lower(in);
	if (s == "private" || s == "inviteonly" || s == "codeonly" || s == "0" || s == "4" || s == "privateunique") return Visibility::Private;
	if (s == "friendsonly" || s == "friends" || s == "1") return Visibility::FriendsOnly;
	if (s == "public" || s == "2") return Visibility::Public;
	if (s == "invisible" || s == "3") return Visibility::Invisible;
	return Visibility::Unknown;
}

const char *to_string(Context c) {
	switch (c) {
		case Context::Offline: return "offline";
		case Context::Lan: return "lan";
		case Context::CustomLobby: return "custom_lobby";
		case Context::QuickMatch: return "quick_match";
		case Context::ServerBrowser: return "server_browser";
		case Context::Online: return "online";
		case Context::OfflinePlay: return "offline_play";
		default: return "unknown";
	}
}

const char *to_string(Visibility v) {
	switch (v) {
		case Visibility::Private: return "private";
		case Visibility::FriendsOnly: return "friends_only";
		case Visibility::Public: return "public";
		case Visibility::Invisible: return "invisible";
		default: return "unknown";
	}
}

static bool allowed_kind(const Observation &o, Tier tier) {
	if (o.context == Context::Lan || o.context == Context::OfflinePlay) return true;
	if (o.context == Context::Offline) return true;
	if (o.context == Context::CustomLobby) {
		if (o.visibility == Visibility::Private || o.visibility == Visibility::FriendsOnly) return true;
		if (tier == Tier::Cosmetic && (o.visibility == Visibility::Public || o.visibility == Visibility::Invisible)) return true;
		return false;
	}
	if (tier == Tier::Cosmetic && (o.context == Context::QuickMatch || o.context == Context::ServerBrowser)) return true;
	return false;
}

static std::vector<const Observation *> fresh_of(const std::vector<Observation> &obs, double now, const Policy &policy) {
	std::vector<const Observation *> fresh;
	for (const auto &o : obs) {
		const double age = now - o.timestamp;
		if (age >= 0.0 && age <= policy.max_age_s) fresh.push_back(&o);
	}
	return fresh;
}



static std::string consistency(const std::vector<const Observation *> &fresh, Context &ctx, Visibility &vis, Visibility &auth_vis) {
	ctx = Context::Unknown;
	for (const auto *o : fresh) {
		if (o->context == Context::Unknown) continue;
		Context c = o->context;
		if (ctx == Context::Unknown) { ctx = c; continue; }
		if (c == ctx) continue;


		auto online_ok = [](Context x) { return x == Context::CustomLobby || x == Context::QuickMatch || x == Context::ServerBrowser; };
		if (c == Context::Online && online_ok(ctx)) continue;
		if (ctx == Context::Online && online_ok(c)) { ctx = c; continue; }
		return std::string("probes disagree (") + to_string(ctx) + " vs " + to_string(c) + ", " + o->source + ")";
	}
	vis = Visibility::Unknown;
	auth_vis = Visibility::Unknown;
	for (const auto *o : fresh) {
		if (o->visibility == Visibility::Unknown) continue;
		if (vis == Visibility::Unknown) vis = o->visibility;
		else if (vis != o->visibility) return "lobby visibility probes disagree (" + o->source + ")";
		if (o->authoritative) auth_vis = o->visibility;
	}
	return "";
}

static std::string label_of(const Observation &o) {
	switch (o.context) {
		case Context::Lan: return "LAN";
		case Context::QuickMatch: return "quick match";
		case Context::ServerBrowser: return "server browser";
		case Context::Offline: return "menu";
		case Context::OfflinePlay:
			if (o.detail == "free_play") return "Free Play";
			if (o.detail == "offline_solo") return "offline solo";
			if (o.detail == "practice") return "practice";
			return "unknown";
		case Context::CustomLobby:
			switch (o.visibility) {
				case Visibility::Private: return "private";
				case Visibility::FriendsOnly: return "friends-only";
				case Visibility::Public: case Visibility::Invisible: return "public";
				default: return "unknown";
			}
		default: return "unknown";
	}
}

std::string mode_label(const std::vector<Observation> &obs, double now, const Policy &policy) {
	auto fresh = fresh_of(obs, now, policy);
	Context ctx; Visibility vis, auth_vis;
	if (fresh.empty() || !consistency(fresh, ctx, vis, auth_vis).empty()) return "unknown";
	for (const auto *o : fresh) {
		if (!o->authoritative || o->context == Context::Unknown) continue;
		Observation m = *o;
		if (m.visibility == Visibility::Unknown) m.visibility = auth_vis;
		return label_of(m);
	}
	return "unknown";
}

Decision decide(const std::vector<Observation> &obs, double now, const Policy &policy, Tier tier) {
	Decision d;
	d.mode = mode_label(obs, now, policy);
	auto fresh = fresh_of(obs, now, policy);
	if (fresh.empty()) { d.reason = "blocked: no fresh observations (fail closed)"; return d; }
	const bool cos = tier == Tier::Cosmetic;


	for (const auto *o : fresh) {



		if (cos) continue;
		if (o->context == Context::QuickMatch) { d.reason = "blocked: quick match/public matchmaking (" + o->source + ")"; return d; }
		if (o->context == Context::ServerBrowser) { d.reason = "blocked: public server-browser game (" + o->source + ")"; return d; }
		if (o->visibility == Visibility::Public) { d.reason = "blocked: public lobby (" + o->source + ")"; return d; }
		if (o->visibility == Visibility::Invisible) { d.reason = "blocked: searchable/invisible lobby treated as public (" + o->source + ")"; return d; }
	}


	Context ctx; Visibility vis, auth_vis;
	std::string why = consistency(fresh, ctx, vis, auth_vis);
	if (!why.empty()) { d.reason = "blocked: " + why; return d; }


	for (const auto *o : fresh) {
		if (!o->authoritative || o->context == Context::Unknown) continue;
		Observation m = *o;
		if (m.visibility == Visibility::Unknown) m.visibility = auth_vis;
		if (m.context == ctx && allowed_kind(m, tier)) {
			d.allowed = true;
			if (m.context == Context::Offline) { d.reason = cos ? "allowed (cosmetic): main menu (local)" : "allowed: main menu (local)"; return d; }
			d.reason = std::string("allowed") + (cos ? " (cosmetic)" : "") + ": " + label_of(m);
			if (o->password_protected) d.reason += " (password)";
			d.reason += " (" + o->source + ")";
			return d;
		}
	}
	d.reason = std::string("blocked: ") + (d.mode == "unknown" ? "lobby/mode not positively identified by an authoritative probe (fail closed)" : d.mode + " is not allowed for this tier");
	return d;
}

bool GateStateMachine::update(const Decision &d, double now) {
	reason_ = d.reason;
	if (!d.allowed) {
		pending_ = false;
		if (active_) { active_ = false; return true; }
		return false;
	}
	if (active_) return false;
	if (!pending_) { pending_ = true; allowed_since_ = now; }
	if (now - allowed_since_ >= enable_after_s_) { active_ = true; pending_ = false; return true; }
	return false;
}

void GateStateMachine::force_off(const std::string &why) {
	pending_ = false;
	active_ = false;
	reason_ = why;
}

static bool parse_ipv4(const std::string &s, unsigned char out[4]) {
	int part = 0;
	size_t i = 0;
	while (part < 4) {
		if (i >= s.size() || !std::isdigit((unsigned char)s[i])) return false;
		int v = 0, digits = 0;
		while (i < s.size() && std::isdigit((unsigned char)s[i])) {
			v = v * 10 + (s[i] - '0');
			if (++digits > 3 || v > 255) return false;
			++i;
		}
		out[part++] = (unsigned char)v;
		if (part < 4) {
			if (i >= s.size() || s[i] != '.') return false;
			++i;
		}
	}
	return i == s.size();
}

bool is_lan_address(const std::string &raw) {
	std::string ip = raw;
	if (ip.rfind("::ffff:", 0) == 0) ip = ip.substr(7);
	unsigned char b[4];
	if (parse_ipv4(ip, b)) {
		if (b[0] == 10) return true;
		if (b[0] == 127) return true;
		if (b[0] == 172 && b[1] >= 16 && b[1] <= 31) return true;
		if (b[0] == 192 && b[1] == 168) return true;
		if (b[0] == 169 && b[1] == 254) return true;
		return false;
	}
	std::string l = ip;
	for (auto &c : l) c = (char)std::tolower((unsigned char)c);
	if (l.find(':') == std::string::npos) return false;
	if (l == "::1") return true;
	if (l.size() >= 4 && l.compare(0, 2, "fe") == 0) {
		char c = l[2];
		if (c == '8' || c == '9' || c == 'a' || c == 'b') return true;
	}
	if (l.size() >= 2 && (l.compare(0, 2, "fc") == 0 || l.compare(0, 2, "fd") == 0)) return true;
	return false;
}

}
