

#include "../src/gate/gameplay_settings.hpp"
#include "../src/gate/goofy_layers.hpp"
#include "../src/gate/touch_rules.hpp"

#include <cmath>
#include <cstdio>

using namespace jvm::gs;
static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static Def fdef(const char *id, double def, double lo, double hi, double step = 0, const char *mod = "goofy") {
	Def d; d.id = id; d.mod = mod; d.type = Type::Float; d.def = { def }; d.min = lo; d.max = hi; d.step = step; d.label = id; return d;
}

int main() {
	Registry r;

	CHECK(r.add(fdef("goofy.gravity_scale", 1.0, 0.05, 5.0, 0.05), "gameplay").ok);
	CHECK(!r.add(fdef("goofy.trail", 1.0, 0, 2), "visual").ok);
	CHECK(r.add(fdef("goofy.trail", 1.0, 0, 2), "visual").error.find("cosmetic") != std::string::npos);
	CHECK(!r.add(fdef("goofy.x", 1.0, 0, 2), "").ok);
	CHECK(!r.add(fdef("other.gravity", 1.0, 0, 2), "gameplay").ok);
	CHECK(!r.add(fdef("goofy.Gravity", 1.0, 0, 2), "gameplay").ok);
	CHECK(!r.add(fdef("goofy.", 1.0, 0, 2), "gameplay").ok);
	CHECK(!r.add(fdef("goofy.bad", 9.0, 0, 2), "gameplay").ok);
	CHECK(!r.add(fdef("goofy.bad", 1.0, 3, 2), "gameplay").ok);
	{ Def d = fdef("goofy.nan", NAN, 0, 2); CHECK(!r.add(d, "gameplay").ok); }
	{ Def d = fdef("rival.gravity_scale", 1.0, 0, 2, 0, "rival"); d.id = "goofy.gravity_scale"; CHECK(!r.add(d, "gameplay").ok); }
	{ Def e; e.id = "goofy.mode"; e.mod = "goofy"; e.type = Type::Enum; e.options = { "normal", "fixed" }; e.def = { 0 }; CHECK(r.add(e, "gameplay").ok); CHECK(r.find("goofy.mode")->max == 1); }
	{ Def e; e.id = "goofy.one"; e.mod = "goofy"; e.type = Type::Enum; e.options = { "a" }; e.def = { 0 }; CHECK(!r.add(e, "gameplay").ok); }
	{ Def b; b.id = "goofy.rules.double_hit_foul"; b.mod = "goofy"; b.type = Type::Bool; b.def = { 1 }; CHECK(r.add(b, "gameplay").ok); }
	{ Def i; i.id = "goofy.rules.max_team_touches"; i.mod = "goofy"; i.type = Type::Int; i.def = { 3 }; i.min = 1; i.max = 10; CHECK(r.add(i, "gameplay").ok); CHECK(r.find(i.id)->step == 1); }
	{ Def v; v.id = "goofy.wind"; v.mod = "goofy"; v.type = Type::Vec; v.dim = 3; v.def = { 0 }; v.min = -5; v.max = 5; CHECK(r.add(v, "gameplay").ok); CHECK(r.find("goofy.wind")->def.size() == 3); }
	CHECK(r.ids().size() == 5 && r.ids()[0] == "goofy.gravity_scale");


	const Def &g = *r.find("goofy.gravity_scale");
	Value out; bool cl = false;
	double big = 99; CHECK(sanitize(g, &big, 1, out, &cl) && out[0] == 5.0 && cl);
	double snap = 1.23; CHECK(sanitize(g, &snap, 1, out) && std::fabs(out[0] - 1.25) < 1e-9);
	double inf = INFINITY; CHECK(!sanitize(g, &inf, 1, out));
	double two[2] = { 1, 2 }; CHECK(!sanitize(g, two, 2, out));
	double half = 0.5; CHECK(sanitize(*r.find("goofy.rules.double_hit_foul"), &half, 1, out) && out[0] == 1.0);
	double frac = 4.6; CHECK(sanitize(*r.find("goofy.rules.max_team_touches"), &frac, 1, out) && out[0] == 5.0);
	double one = 1; CHECK(sanitize(*r.find("goofy.wind"), &one, 1, out) && out.size() == 3);


	CHECK(r.set_local("goofy.gravity_scale", { 0.5 }).ok);
	CHECK(!r.set_local("goofy.nope", { 1 }).ok);
	CHECK(!r.set_local("goofy.gravity_scale", { NAN }).ok);
	std::string src;
	CHECK(r.role() == Role::Off && r.get("goofy.gravity_scale", &src)[0] == 1.0 && src == "stock");
	r.set_role(Role::Local);
	CHECK(r.get("goofy.gravity_scale", &src)[0] == 0.5 && src == "local");
	CHECK(r.get("goofy.rules.max_team_touches", &src)[0] == 3 && src == "default");
	CHECK(r.set_local("goofy.gravity_scale", { 1.0 }).ok && r.local("goofy.gravity_scale") == nullptr);
	r.set_local("goofy.gravity_scale", { 0.5 });
	auto snap0 = r.snapshot();
	CHECK(snap0.size() == 5 && snap0["goofy.gravity_scale"][0] == 0.5);
	std::string bytes = Registry::encode(snap0);
	CHECK(bytes.rfind("JVGS 1\n", 0) == 0 && Registry::hash(bytes) == Registry::hash(Registry::encode(r.snapshot())));
	r.set_role(Role::Client);
	CHECK(r.snapshot().empty());
	CHECK(r.locked() && r.get("goofy.gravity_scale", &src)[0] == 1.0 && src == "stock");
	ApplyResult ar = r.apply_host("JVGS 1\ngoofy.gravity_scale=7\ngoofy.rules.double_hit_foul=0\nmissing.mod=1\ngoofy.wind=1,2\n");
	CHECK(ar.ok && ar.applied == 2 && ar.unknown == 1 && ar.rejected == 1);
	CHECK(r.get("goofy.gravity_scale", &src)[0] == 5.0 && src == "host");
	CHECK(r.get("goofy.rules.double_hit_foul")[0] == 0.0);
	CHECK(!r.apply_host("JVGS 2\ngoofy.gravity_scale=1\n").ok);
	CHECK(!r.apply_host("hello").ok);
	CHECK(!r.apply_host("JVGS 1\ngoofy.gravity_scale=OS.execute()\n").ok);
	CHECK(!r.apply_host(std::string("JVGS 1\n") + std::string(40000, 'x')).ok);
	CHECK(r.get("goofy.gravity_scale")[0] == 5.0);
	r.apply_host("JVGS 1\ngoofy.rules.max_team_touches=4\n");
	CHECK(r.get("goofy.gravity_scale")[0] == 1.0 && r.get("goofy.rules.max_team_touches")[0] == 4);
	r.set_role(Role::Off);
	CHECK(r.get("goofy.rules.max_team_touches")[0] == 3);
	r.set_role(Role::Local);
	CHECK(r.host_values().empty() && r.get("goofy.gravity_scale")[0] == 0.5);

	std::map<std::string, Value> dec;
	CHECK(decode(Registry::encode(r.snapshot()), dec) && dec["goofy.gravity_scale"][0] == 0.5 && dec["goofy.wind"].size() == 3);

	CHECK(r.add(fdef("goofy.gravity_scale", 1.0, 0.75, 5.0, 0.05), "gameplay").ok && r.local("goofy.gravity_scale")[0][0] == 0.75);
	CHECK(r.remove_mod("goofy") == 5 && r.ids().empty() && r.get("goofy.gravity_scale").empty());


	using namespace jvm::rules;
	Streak s;
	streak_after_touch(s, 11, 1, 1);
	CHECK(s.count == 1 && !may_touch_again(s, true, 1));
	CHECK(may_touch_again(s, true, 2) && may_touch_again(s, false, 1));
	streak_after_touch(s, 11, 1, 2);
	CHECK(s.count == 2 && !may_touch_again(s, true, 2) && may_touch_again(s, true, 3) && may_touch_again(s, false, 2));
	streak_after_touch(s, 12, 1, 3);
	CHECK(s.who == 12 && s.count == 1);
	streak_after_touch(s, 12, 1, 0);
	CHECK(s.who == 0 && may_touch_again(s, true, 1));
	streak_after_touch(s, 12, -1, 1); streak_after_touch(s, 12, 1, 1);
	CHECK(s.count == 1 && s.side == 1);
	CHECK(clamp_consecutive(0) == 1 && clamp_consecutive(99) == 10 && team_touches_to_max_hits(3) == 0 && team_touches_to_max_hits(5) == 5 && team_touches_to_max_hits(0) == 1);


	{
		Manifest m{ { "goofy", "1" }, { "low_grav", "2.1" } };
		std::set<std::string> off{ "low_grav" }, off_back;
		std::string b = encode_manifest(m, true, &off);
		Manifest back; bool lo = false; std::string why;
		CHECK(b.rfind("JVGM 1\nloader=1\n", 0) == 0 && b.find("!low_grav=1\n") != std::string::npos);
		CHECK(decode_manifest(b, back, lo, &why, &off_back) && back == m && lo && off_back == off);
		CHECK(decode_manifest(encode_manifest(m, false), back, lo, nullptr, &off_back) && !lo && off_back.empty());
		CHECK(decode_manifest("JVGM 1\ngoofy=1\n", back, lo) && !lo);
		CHECK(!decode_manifest("JVGM 2\nloader=1\n", back, lo) && !decode_manifest("xx", back, lo) && !decode_manifest("JVGM 1\nBad Name=1\n", back, lo) && !decode_manifest("JVGM 1\n!Bad=1\n", back, lo));
		CHECK(version_major("1.3.0") == "1" && version_major("2") == "2");
		CHECK(mod_active("goofy", "1.0", m, &why) && why.empty());
		CHECK(!mod_active("shots", "1", m, &why) && why == "missing");
		CHECK(!mod_active("low_grav", "3.0", m, &why) && why.find("version 2.1") == 0);


		Registry g;
		g.add(fdef("goofy.gravity_scale", 1.0, 0.05, 5.0), "gameplay");
		g.add(fdef("low_grav.gravity", 1.0, 0.1, 3.0, 0, "low_grav"), "gameplay");
		g.set_local_mods({ { "goofy", "1" }, { "low_grav", "2" } });
		g.set_local("goofy.gravity_scale", { 2.0 }); g.set_local("low_grav.gravity", { 0.5 });
		CHECK(!g.mod_active_for("goofy"));
		g.set_role(Role::Local);
		g.set_peer("p2", PeerInfo{ { { "goofy", "1" }, { "low_grav", "2" } }, true });
		g.set_peer("p3", PeerInfo{ { { "goofy", "1.4" } }, true });
		g.set_peer("p4", PeerInfo{ { { "goofy", "1" }, { "low_grav", "3" } }, true });
		CHECK(g.mod_active_for("low_grav"));
		CHECK(g.preview_disabled().count("low_grav") && g.preview_disabled().at("low_grav").size() == 2);
		auto &dis = g.lock_match();
		CHECK(g.match_locked() && dis.size() == 1 && dis.at("low_grav")[0].player == "p3" && dis.at("low_grav")[0].why == "missing" && dis.at("low_grav")[1].why.find("version 3") == 0);
		CHECK(!g.mod_active_for("low_grav", "", &why) && why.find("off for this match: p3 (missing)") == 0);
		CHECK(!g.mod_active_for("low_grav", "p2") && g.mod_active_for("goofy", "p3") && g.mod_active_for("goofy"));
		std::string src;
		CHECK(g.get("low_grav.gravity", &src)[0] == 1.0 && src == "stock" && g.get("goofy.gravity_scale")[0] == 2.0);
		auto snap = g.snapshot();
		CHECK(snap.size() == 1 && snap.count("goofy.gravity_scale"));
		g.set_peer("late", PeerInfo{ {}, true });
		CHECK(g.is_late("late") && g.mod_active_for("goofy") && !g.preview_disabled().count("goofy"));
		g.unlock_match();
		CHECK(g.mod_active_for("low_grav") && g.get("low_grav.gravity")[0] == 0.5);
		g.lock_match();
		CHECK(!g.mod_active_for("goofy") && !g.is_late("late"));
		g.remove_peer("late"); g.remove_peer("p3"); g.remove_peer("p4"); g.unlock_match(); g.lock_match();
		CHECK(g.disabled().empty() && g.mod_active_for("goofy") && g.mod_active_for("low_grav"));
		g.set_role(Role::Client);
		CHECK(g.peers().empty() && !g.match_locked());
		CHECK(!g.mod_active_for("goofy", "", &why));
		g.apply_host_values({ { "goofy.gravity_scale", { 3.0 } }, { "low_grav.gravity", { 2.0 } } });
		g.set_host_mods({ { "goofy", "1.0" }, { "low_grav", "2" } }, { "low_grav" });
		CHECK(g.mod_active_for("goofy") && !g.mod_active_for("low_grav", "", &why) && why == "off for this match: a player does not have it");
		CHECK(g.match_disabled("low_grav") && !g.match_disabled("goofy"));
		CHECK(g.get("low_grav.gravity", &src)[0] == 1.0 && src == "stock" && g.get("goofy.gravity_scale")[0] == 3.0);
		g.set_host_mods({ { "goofy", "1.0" } }, {});
		CHECK(!g.mod_active_for("low_grav", "", &why) && why == "off for this match: the host does not have it" && g.match_disabled("low_grav"));
		g.set_host_mods({ { "goofy", "2.0" }, { "low_grav", "2" } }, {});
		CHECK(!g.mod_active_for("goofy") && g.get("goofy.gravity_scale")[0] == 1.0 && g.get("low_grav.gravity")[0] == 2.0);
		g.set_role(Role::Local);
		CHECK(!g.host_mods_known());
	}

	std::printf("%d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
