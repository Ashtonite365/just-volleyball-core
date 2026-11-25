

#include "../src/api/api_core.hpp"

#include <cstdio>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

using namespace jvm::api;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static GateView gv(const std::string &mode, bool full, bool cos, bool pin = true) {
	GateView g; g.pin_ok = pin; g.full = full && pin; g.cosmetic = cos && pin; g.mode = mode; return g;
}

struct ModeRow { const char *mode; bool full, cosmetic; };
static const ModeRow kModes[] = {
	{ "Free Play", true, true }, { "practice", true, true }, { "offline solo", true, true }, { "menu", true, true },
	{ "LAN", true, true }, { "private", true, true }, { "friends-only", true, true },
	{ "public", false, true }, { "quick match", false, true }, { "server browser", false, true }, { "unknown", false, false },
};

static std::string slurp(const std::string &p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }

int main(int argc, char **argv) {
	const std::string repo = argc > 1 ? argv[1] : ".";

	const auto &caps = capabilities();
	CHECK(caps.size() >= 40);
	std::set<std::string> ids;
	std::regex idre("^[a-z]+(\\.[a-z_]+)+$");
	std::regex refre("^game/[A-Za-z0-9_/]+\\.gd:[0-9]+ [A-Za-z_][A-Za-z0-9_]*$");
	for (auto &c : caps) {
		CHECK(ids.insert(c.id).second);
		CHECK(std::regex_match(c.id, idre));
		CHECK(std::string(c.since) == "1.0" || std::string(c.since) == "1.1" || std::string(c.since) == "1.2" || std::string(c.since) == "1.3" || std::string(c.since) == "1.4");
		CHECK(std::string(c.feature).size() > 10);
		CHECK(std::string(c.functions).size() > 2);
		CHECK(find(c.id) == &c);
		std::string refs = c.game_refs;
		if (refs != "-") {
			std::stringstream ss(refs); std::string r;
			while (std::getline(ss, r, ';')) {
				if (!r.empty() && r[0] == ' ') r = r.substr(1);
				bool ok = std::regex_match(r, refre);
				if (!ok) std::printf("bad ref in %s: '%s'\n", c.id, r.c_str());
				CHECK(ok);
			}
		}

		if (c.scope != Scope::Local) CHECK(c.access == Access::Gameplay || c.access == Access::Sync || c.access == Access::LocalUi);
		if (c.scope == Scope::Peers) CHECK(c.access == Access::Sync);
	}
	CHECK(find("nope.nothing") == nullptr);
	CHECK(find("ball.physics") && find("ball.physics")->access == Access::Gameplay);
	CHECK(find("net.lobby_data") && find("net.lobby_data")->access == Access::Sync);
	CHECK(find("net.content_download") && find("net.content_download")->status == Status::Impl && find("net.content_download")->access == Access::Sync);
	CHECK(find("net.content_offer") && find("net.content_offer")->status == Status::Impl && find("net.content_offer")->access == Access::Sync);
	CHECK(find("stage.lobby_picker") && find("stage.lobby_picker")->status == Status::Planned);


	for (auto &m : kModes) {
		GateView g = gv(m.mode, m.full, m.cosmetic);
		const std::string mode = m.mode;
		const bool known = mode != "unknown";
		const bool sync = mode == "LAN" || mode == "private" || mode == "friends-only";
		const bool offline = mode == "Free Play" || mode == "practice" || mode == "offline solo" || mode == "menu";
		CHECK(access_open(Access::Read, g));
		CHECK(access_open(Access::LocalUi, g));
		CHECK(access_open(Access::Cosmetic, g) == known);
		CHECK(access_open(Access::Gameplay, g) == (offline || sync));
		CHECK(access_open(Access::Sync, g) == sync);
		CHECK(access_open(Access::Offline, g) == offline);

		GateView np = gv(m.mode, m.full, m.cosmetic, false);
		for (Access a : { Access::Read, Access::LocalUi, Access::Cosmetic, Access::Gameplay, Access::Sync, Access::Offline }) CHECK(!access_open(a, np));
	}
	{
		std::string why;
		CHECK(!access_open(Access::Sync, gv("Free Play", true, true), &why) && why.find("LAN / private / friends-only") != std::string::npos);
		CHECK(!access_open(Access::Offline, gv("LAN", true, true), &why) && why.find("offline only") != std::string::npos);
		GateView pub = gv("public", false, true); pub.full_reason = "blocked: public lobby";
		CHECK(!access_open(Access::Gameplay, pub, &why) && why == "blocked: public lobby");
		CHECK(!access_open(Access::Read, gv("menu", true, true, false), &why) && why.find("not pinned") != std::string::npos);
	}


	CHECK(version_string() == "1.4.0");
	CHECK(version_compatible("1"));
	CHECK(version_compatible("1.0"));
	CHECK(version_compatible("1.0.7"));
	CHECK(version_compatible("1.1"));
	CHECK(version_compatible("1.2"));
	CHECK(version_compatible("1.3"));
	CHECK(version_compatible("1.4"));
	CHECK(!version_compatible("1.5"));
	CHECK(!version_compatible("2.0"));
	CHECK(!version_compatible("0.9"));
	CHECK(!version_compatible(""));
	CHECK(!version_compatible("1.x"));
	CHECK(!version_compatible("1..0"));
	{ std::string why; CHECK(!version_compatible("2.0", &why) && why.find("major") != std::string::npos); }


	Registry reg;
	RegisterResult rr = reg.register_mod("goofy", "1.0", { "ball.physics", "engine.time_scale", "stage.lobby_picker", "bogus.cap" });
	CHECK(rr.ok);
	CHECK(rr.granted.size() == 2);
	CHECK(rr.denied.size() == 2);
	CHECK(rr.denied[0].find("planned") != std::string::npos);
	CHECK(rr.denied[1].find("unknown") != std::string::npos);
	CHECK(!reg.register_mod("old", "2.0", { "ball.physics" }).ok);
	CHECK(!reg.registered("old"));
	CHECK(!reg.register_mod("", "1.0", {}).ok);
	GateView fp = gv("Free Play", true, true), pub = gv("public", false, true), lan = gv("LAN", true, true);
	CHECK(reg.check("goofy", "ball.physics", fp).ok);
	CHECK(reg.check("goofy", "ball.physics", lan).ok);
	CheckResult cr = reg.check("goofy", "ball.physics", pub);
	CHECK(!cr.ok && cr.why.find("gameplay") != std::string::npos);
	cr = reg.check("goofy", "rules.touch_count", fp);
	CHECK(!cr.ok && cr.why.find("did not declare") != std::string::npos);
	cr = reg.check("someone", "ball.physics", fp);
	CHECK(!cr.ok && cr.why.find("not registered") != std::string::npos);
	cr = reg.check("goofy", "nope.cap", fp);
	CHECK(!cr.ok && cr.why.find("unknown") != std::string::npos);
	CHECK(reg.mods().at("goofy").calls.at("ball.physics") == 2);
	CHECK(reg.mods().at("goofy").refusals.at("ball.physics") == 1);
	CHECK(reg.peek("goofy", "ball.physics", fp).ok);
	CHECK(reg.mods().at("goofy").calls.at("ball.physics") == 2);
	CHECK(reg.check("goofy", "ball.physics", pub, false).ok);
	CHECK(reg.mods().at("goofy").calls.at("ball.physics") == 3);
	CHECK(!reg.check("goofy", "rules.touch_count", pub, false).ok);
	CHECK(!reg.check("other", "ball.physics", pub, false).ok);
	CHECK(!reg.peek("goofy", "ball.physics", pub).ok);

	CHECK(reg.register_mod("goofy", "1", { "rules.touch_count" }).ok);
	CHECK(!reg.check("goofy", "ball.physics", fp).ok);
	CHECK(reg.check("goofy", "rules.touch_count", fp).ok);

	CHECK(reg.register_mod("sync", "1.0", { "net.lobby_data" }).ok);
	CHECK(reg.check("sync", "net.lobby_data", lan).ok);
	CHECK(reg.check("sync", "net.lobby_data", gv("private", true, true)).ok);
	CHECK(!reg.check("sync", "net.lobby_data", fp).ok);
	CHECK(!reg.check("sync", "net.lobby_data", pub).ok);
	CHECK(!reg.check("sync", "net.lobby_data", gv("quick match", false, true)).ok);


	CHECK(library_allowed("shots", false));
	CHECK(library_allowed("goofy", true));
	CHECK(library_allowed("shots", true));
	CHECK(library_allowed("loader", true));
	CHECK(library_allowed("textures", true));
	CHECK(library_allowed("models", true));
	{ std::string why; CHECK(!library_allowed("legacy_thing", true, &why) && why.find("not migrated") != std::string::npos); }


	Coverage cov = coverage(&reg);
	CHECK(cov.total == int(caps.size()));
	CHECK(cov.impl + cov.mapped + cov.planned == cov.total);
	CHECK(cov.impl >= 20);
	CHECK(cov.game_refs >= 100);
	CHECK(cov.used_by_mods == 2);
	std::printf("coverage: %d capabilities (%d impl, %d mapped, %d planned), %d game refs\n", cov.total, cov.impl, cov.mapped, cov.planned, cov.game_refs);


	CHECK(goofy_ball_param_count() == 156);
	CHECK(goofy_params().size() == 156 + 195);
	{
		int phys = 0, feel = 0, ext = 0, pfeel = 0, pext = 0, ints = 0;
		std::set<std::string> gids;
		for (auto &p : goofy_params()) {
			CHECK(gids.insert(p.id).second);
			std::string t = p.target;
			if (goofy_is_player_library(p)) {
				CHECK(std::string(p.id).rfind("player.", 0) == 0);
				pfeel += t == "FEEL"; pext += t == "EXT"; ints += std::string(p.apply) == "live_int";
			} else { phys += t == "PHYS"; feel += t == "FEEL"; ext += t == "EXT"; }
			CHECK(p.min <= p.max);
			if (!p.is_color) CHECK(p.v[0] >= p.min && p.v[0] <= p.max);
		}
		CHECK(phys == 124 && feel == 27 && ext == 5);
		CHECK(pfeel == 194 && pext == 1 && ints == 1);
		const GoofyParam *ms = find_goofy_param("player.move_speed");
		CHECK(ms && goofy_is_player_library(*ms) && ms->v[0] == 5.9);
		const GoofyParam *bs = find_goofy_param("player.body_scale");
		CHECK(bs && std::string(bs->target) == "EXT" && bs->min == 0.25 && bs->max == 4.0);
		CHECK(!goofy_is_player_library(*find_goofy_param("player.reach")));
		const GoofyParam *g = find_goofy_param("ball.gravity");
		CHECK(g && std::string(g->prop) == "gravity" && g->v[0] == 11.0);
		CHECK(!find_goofy_param("not.a.param"));
	}
	for (auto &e : events()) { CHECK(find(e.cap) != nullptr); CHECK(find_event(e.name) == &e); }
	CHECK(events().size() == 17);
	CHECK(allowed_signal("ball", "struck") && allowed_signal("player", "touched") && allowed_signal("net", "lobby_changed"));
	CHECK(!allowed_signal("net", "input_received") && !allowed_signal("net", "snapshot_received") && !allowed_signal("net", "chat"));
	CHECK(!allowed_signal("ball", "rpc") && !allowed_signal("nope", "struck"));
	CHECK(std::string(library_path("goofy")) == "/root/JVGoofyMods" && std::string(library_path("bogus")).empty());
	CHECK(mode_labels().size() == 11);
	{
		for (auto &c : caps) {
			std::string f = c.functions;
			if (c.access == Access::Read) CHECK(f.find("_set(") == std::string::npos && f.find("set_override") == std::string::npos && f.find("_apply") == std::string::npos);
		}
		CHECK(find("goofy.ball")->access == Access::Offline);
		CHECK(find("goofy.rules")->access == Access::Gameplay && find("goofy.rules")->scope == Scope::Host);
		CHECK(find("goofy.player")->access == Access::Offline && find("goofy.ball_live")->access == Access::Offline);
		CHECK(find("goofy.host_sync")->access == Access::Sync && find("goofy.host_sync")->scope == Scope::Host);
		CHECK(find("player.tuning")->access == Access::Gameplay && find("goofy.events")->access == Access::Read);
		CHECK(find("stage.pick")->access == Access::Cosmetic && find("court.env")->access == Access::Cosmetic);
		CHECK(find("menu.perf")->access == Access::LocalUi && find("ui.mods_panel")->access == Access::LocalUi);
		CHECK(find("shots.arm")->access == Access::Gameplay);
	}


	{
		const std::string core_md = slurp(repo + "/docs/CORE-API.md");
		CHECK(!core_md.empty());
		std::string src = slurp(repo + "/src/core/mod_api.cpp") + slurp(repo + "/src/core/mod_api_core.cpp") + slurp(repo + "/src/core/mod_api_dev.cpp") + slurp(repo + "/src/core/gameplay_settings_api.cpp");
		std::regex bm("D_METHOD\\(\"([a-z_]+)\"|BM\\(([a-z_]+)");
		std::set<std::string> fns;
		for (std::sregex_iterator it(src.begin(), src.end(), bm), end; it != end; ++it) {
			std::string n = (*it)[1].matched ? (*it)[1].str() : (*it)[2].str();
			if (n.empty() || n[0] == '_' || n == "name") continue;
			fns.insert(n);
		}
		int missing = 0;
		for (auto &n : fns) if (core_md.find("`" + n + "(") == std::string::npos) { std::printf("CORE-API.md misses function %s\n", n.c_str()); missing++; }
		CHECK(missing == 0);
		CHECK(fns.size() >= 120);
		for (const char *sig : { "access_changed", "event" }) CHECK(core_md.find(std::string("`") + sig + "(") != std::string::npos);
		for (auto &e : events()) CHECK(core_md.find(std::string("`") + e.name + "`") != std::string::npos);
		for (auto &c : caps) CHECK(core_md.find(std::string("`") + c.id + "`") != std::string::npos);
		std::printf("CORE-API.md: %zu bound functions checked\n", fns.size());
	}


	const std::string api_md = slurp(repo + "/docs/API.md"), rules_md = slurp(repo + "/rules.md");
	CHECK(!api_md.empty());
	CHECK(!rules_md.empty());
	for (auto &c : caps) {
		const std::string tick = std::string("`") + c.id + "`";
		bool a = api_md.find(tick) != std::string::npos, r = rules_md.find(tick) != std::string::npos;
		if (!a || !r) std::printf("capability %s missing from %s\n", c.id, !a ? "docs/API.md" : "rules.md");
		CHECK(a && r);
	}

	std::printf("RESULT api unit: %d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
