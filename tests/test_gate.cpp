#include "../src/gate/gate_logic.hpp"
#include "../src/gate/shot_math.hpp"
#include "../src/gate/skin_sync.hpp"
#include "../src/gate/version_pin.hpp"
#include "../src/gate/touch_rules.hpp"
#include "../src/gate/model_rules.hpp"
#include "../src/common/accessory_rules.hpp"
#include "../src/gate/goofy_layers.hpp"
#include "../src/gate/content_share.hpp"
#include <cmath>

#include <cmath>
#include <cstdio>
#include <set>
#include <string>

using namespace jvm::gate;

static int g_fail = 0, g_pass = 0;
static jvm::shots::Combo C2(const char *tr, std::vector<std::string> hold, const char *ctx, const char *touch, const char *air) {
	jvm::shots::Combo c; c.trigger = tr; c.hold = std::move(hold); c.context = ctx; c.touch = touch; c.air = air; return c;
}
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static Observation ob(const char *src, Context c, Visibility v = Visibility::Unknown, bool auth = true, double t = 10.0, bool pw = false) {
	Observation o; o.source = src; o.context = c; o.visibility = v; o.authoritative = auth; o.timestamp = t; o.password_protected = pw;
	return o;
}
static bool allowed(std::vector<Observation> v, double now = 10.0) { return decide(v, now).allowed; }
static bool cosmetic(std::vector<Observation> v, double now = 10.0) { return decide(v, now, Policy(), Tier::Cosmetic).allowed; }
static Observation off(const char *detail, bool auth = true) { Observation o = ob("jv-net", Context::OfflinePlay, Visibility::Unknown, auth); o.detail = detail; return o; }
static std::string mode(std::vector<Observation> v) { return mode_label(v, 10.0); }


struct FakeRally { int side = 0, count = 0; bool fault = false; };
static void game_touch(FakeRally &r, int side) {
	if (r.count >= jvm::rules::kGameTouches && side == r.side) { r.fault = true; return; }
	if (side != r.side) { r.count = 0; r.side = side; }
	r.count++;
}
static int touches_before_fault(int max_hits) {
	FakeRally r; jvm::rules::Shadow sh;
	for (int i = 1; i <= 20; i++) {
		game_touch(r, 1);
		if (r.fault) return i - 1;
		int w = jvm::rules::after_touch(sh, r.side, r.count, max_hits);
		if (w >= 0) r.count = w;
	}
	return 20;
}

int main() {

	CHECK(allowed({ ob("game", Context::Lan) }));
	CHECK(allowed({ ob("game", Context::CustomLobby, Visibility::Private) }));
	CHECK(allowed({ ob("game", Context::CustomLobby, Visibility::Private, true, 10.0, true) }));
	CHECK(allowed({ ob("game", Context::CustomLobby, Visibility::FriendsOnly) }));

	CHECK(!allowed({ ob("game", Context::CustomLobby, Visibility::Public) }));
	CHECK(!allowed({ ob("game", Context::CustomLobby, Visibility::Public, true, 10.0, true) }));
	CHECK(!allowed({ ob("game", Context::CustomLobby, Visibility::Invisible) }));
	CHECK(!allowed({ ob("game", Context::QuickMatch) }));
	CHECK(!allowed({ ob("game", Context::ServerBrowser) }));
	CHECK(allowed({ ob("game", Context::Offline) }));
	CHECK(!allowed({ ob("game", Context::Online) }));

	CHECK(!allowed({}));
	CHECK(!allowed({ ob("game", Context::Unknown) }));
	CHECK(!allowed({ ob("game", Context::CustomLobby, Visibility::Unknown) }));
	CHECK(!allowed({ ob("net", Context::Lan, Visibility::Unknown,  false) }));
	CHECK(!allowed({ ob("game", Context::Lan, Visibility::Unknown, true, 1.0) }, 10.0));
	CHECK(!allowed({ ob("game", Context::Lan, Visibility::Unknown, true, 11.0) }, 10.0));

	CHECK(!allowed({ ob("game", Context::CustomLobby, Visibility::Private), ob("steam", Context::Unknown, Visibility::Public, false) }));
	CHECK(!allowed({ ob("game", Context::Lan), ob("net", Context::Online, Visibility::Unknown, false) }));
	CHECK(!allowed({ ob("game", Context::Lan), ob("other", Context::QuickMatch, Visibility::Unknown, false) }));
	CHECK(!allowed({ ob("game", Context::CustomLobby, Visibility::Private), ob("steam", Context::Unknown, Visibility::FriendsOnly, false) }));
	CHECK(allowed({ ob("game", Context::CustomLobby, Visibility::Private), ob("net", Context::Online, Visibility::Unknown, false) }));
	CHECK(!allowed({ ob("game", Context::CustomLobby), ob("steam", Context::Unknown, Visibility::FriendsOnly, false) }));
	CHECK(allowed({ ob("game", Context::CustomLobby), ob("game-vis", Context::Unknown, Visibility::FriendsOnly, true) }));
	CHECK(!allowed({ ob("game-vis", Context::Unknown, Visibility::Private, true) }));
	CHECK(allowed({ ob("game", Context::Lan), ob("net", Context::Lan, Visibility::Unknown, false) }));
	CHECK(!allowed({ ob("game", Context::Lan), ob("game2", Context::CustomLobby, Visibility::Private) }));

	CHECK(allowed({ ob("game", Context::CustomLobby, Visibility::Private, true, 10.0), ob("old", Context::QuickMatch, Visibility::Unknown, false, 1.0) }));


	CHECK(allowed({ off("free_play") }));
	CHECK(allowed({ off("offline_solo") }));
	CHECK(allowed({ off("practice") }));
	CHECK(!allowed({ off("free_play", false) }));
	CHECK(!allowed({ off("free_play"), ob("net", Context::Online, Visibility::Unknown, false) }));
	CHECK(!allowed({ off("free_play"), ob("net", Context::Lan, Visibility::Unknown, false) }));
	CHECK(!allowed({ off("free_play"), ob("x", Context::CustomLobby, Visibility::Public, false) }));

	CHECK(allowed({ ob("jv-net", Context::Offline) }));
	CHECK(cosmetic({ ob("jv-net", Context::Offline) }));
	CHECK(decide({ ob("jv-net", Context::Offline) }, 10.0).reason == "allowed: main menu (local)");
	CHECK(decide({ ob("jv-net", Context::Offline) }, 10.0, Policy(), Tier::Cosmetic).reason == "allowed (cosmetic): main menu (local)");
	CHECK(!allowed({ ob("jv-net", Context::Offline, Visibility::Unknown, false) }));
	CHECK(!allowed({ ob("jv-net", Context::Offline), ob("net", Context::Online, Visibility::Unknown, false) }));
	CHECK(!allowed({ ob("jv-net", Context::Offline), ob("x", Context::CustomLobby, Visibility::Public, false) }));
	CHECK(decide({ ob("jv-net", Context::Offline) }, 10.0, Policy(), Tier::Cosmetic).mode == "menu");
	CHECK(!cosmetic({ ob("jv-net", Context::Offline, Visibility::Unknown, false) }));
	CHECK(!cosmetic({ ob("jv-net", Context::Offline, Visibility::Unknown, true, 1.0) }));
	CHECK(!cosmetic({ ob("jv-net", Context::Offline), ob("net", Context::Online, Visibility::Unknown, false) }));
	CHECK(!cosmetic({ ob("jv-net", Context::Offline), ob("net", Context::Lan, Visibility::Unknown, false) }));
	CHECK(!cosmetic({ ob("jv-net", Context::Offline), ob("x", Context::CustomLobby, Visibility::Public) }));
	CHECK(!cosmetic({ ob("jv-net", Context::Unknown) }));
	CHECK(!allowed({ ob("jv-net", Context::Offline), ob("x", Context::Lan) }));

	CHECK(cosmetic({ ob("g", Context::CustomLobby, Visibility::Public) }));
	CHECK(cosmetic({ ob("g", Context::CustomLobby, Visibility::Invisible) }));
	CHECK(cosmetic({ ob("g", Context::QuickMatch) }));
	CHECK(cosmetic({ ob("g", Context::ServerBrowser) }));
	CHECK(cosmetic({ ob("g", Context::CustomLobby, Visibility::Private) }));
	CHECK(cosmetic({ ob("g", Context::Lan) }));
	CHECK(cosmetic({ off("practice") }));
	CHECK(!cosmetic({}));
	CHECK(!cosmetic({ ob("g", Context::CustomLobby, Visibility::Unknown) }));
	CHECK(!cosmetic({ ob("g", Context::Unknown) }));
	CHECK(!cosmetic({ ob("g", Context::Lan), ob("g2", Context::QuickMatch, Visibility::Unknown, false) }));
	CHECK(!allowed({ ob("g", Context::QuickMatch) }) && cosmetic({ ob("g", Context::QuickMatch) }));

	CHECK(touches_before_fault(0) == 3);
	CHECK(touches_before_fault(3) == 3);
	CHECK(touches_before_fault(1) == 1);
	CHECK(touches_before_fault(2) == 2);
	CHECK(touches_before_fault(4) == 4);
	CHECK(touches_before_fault(7) == 7);
	CHECK(touches_before_fault(10) == 10);
	CHECK(touches_before_fault(99) == 10);
	CHECK(jvm::rules::clamp_max_hits(-3) == 0);
	{
		FakeRally r; jvm::rules::Shadow sh; int seq[] = { 1, 1, 1, 1, -1, 1, 1, 1, 1 };
		for (int side : seq) { game_touch(r, side); if (r.fault) break; int w = jvm::rules::after_touch(sh, r.side, r.count, 4); if (w >= 0) r.count = w; }
		CHECK(!r.fault && sh.count == 4 && r.count == 3);
		game_touch(r, 1); CHECK(r.fault);
	}
	{
		FakeRally r; jvm::rules::Shadow sh;
		for (int i = 0; i < 3; i++) { game_touch(r, 1); int w = jvm::rules::after_touch(sh, r.side, r.count, 5); if (w >= 0) r.count = w; }
		r.count = 0; game_touch(r, 1); jvm::rules::after_touch(sh, r.side, r.count, 5);
		CHECK(sh.count == 1);
	}

	{
		auto shots = jvm::shots::default_shots();
		CHECK(shots.size() == 11 && shots[9].id == "curveball" && shots[10].id == "float" && shots[10].rally_float && shots[0].default_key == "KEY_KP_1" && shots[8].default_key == "KEY_KP_9");
		int d = -1; CHECK(jvm::shots::parse_key_name("KEY_KP_5", d) && d == 5);
		CHECK(jvm::shots::parse_key_name("KP_9", d) && d == 9);
		CHECK(!jvm::shots::parse_key_name("KEY_1", d));
		const auto &top = shots[2], &fl = shots[4], &hs = shots[0];
		auto g = jvm::shots::game_launch(top, { 0, 0, -1 });
		CHECK(g.dive_g > 1.5 && !g.start_float && g.vel.z < -10);
		g = jvm::shots::game_launch(fl, { 1, 0, 0 });
		CHECK(g.start_float && g.float_charge >= 0.0 && g.float_charge < 0.5 && g.vel.x > 10 && g.dive_g == 1.0);
		g = jvm::shots::game_launch(hs, { 0, 0, 1 });
		CHECK(g.vel.y > g.vel.z && !g.start_float);
		CHECK(jvm::shots::touch_matches(fl, true) && !jvm::shots::touch_matches(fl, false));
		CHECK(jvm::shots::touch_matches(hs, false) && !jvm::shots::touch_matches(hs, true));
	}

	{
		using namespace jvm::shots;
		auto shots = default_shots();
		auto T = [](bool serving, bool set, bool air, double power, std::vector<std::string> held, bool fwd = false) {
			TouchState t; t.serving = serving; t.set_touch = set; t.airborne = air; t.power = power; t.held = held; t.forward_armed = fwd; return t;
		};
		auto id = [&](const TouchState &t) { int i = select_shot(shots, t); return i < 0 ? std::string("-") : shots[i].id; };

		CHECK(id(T(false, false, false, 0.7, {})) == "-");
		CHECK(id(T(false, false, false, 0.7, { "sprint" })) == "-");
		CHECK(id(T(false, false, true, 1.0, { "strafe" })) == "-");
		CHECK(id(T(true, false, false, 0.7, {})) == "-");
		CHECK(id(T(true, false, true, 1.0, { "move_back" })) == "-");

		CHECK(id(T(false, true, false, 0.5, { "sprint", "strafe" })) == "high_shot");
		CHECK(id(T(false, false, false, 0.5, { "sprint", "strafe" })) == "low_shot");
		CHECK(id(T(false, false, true, 1.0, { "sprint", "strafe" })) == "low_shot");

		CHECK(id(T(false, false, false, 0.5, { "move_forward" })) == "-");
		CHECK(id(T(false, false, true, 0.5, { "move_forward" })) == "topspin");
		CHECK(id(T(false, true, true, 0.5, { "move_forward" })) == "topspin");
		CHECK(id(T(false, false, true, 0.5, {})) == "-");
		CHECK(id(T(true, false, true, 0.5, { "move_forward" })) == "-");
		CHECK(id(T(false, false, true, 0.5, { "sprint", "strafe", "move_forward" })) == "low_shot");

		{
			SpinParams sp; Court ct;
			SpinPath pa = spin_path({ -10.3, 0, 4.73 }, { -1.0, 0, -0.09 }, { 1.0, 0, 0 }, 1.0, sp, ct);
			CHECK(pa.L > 1.0 && pa.at(1.0).x > pa.c.x + 1.0 && pa.at(1.0).x <= ct.half_w + 1e-6);
			SpinPath pb = spin_path({ -10.3, 0, 4.73 }, { -1.0, 0, -0.09 }, { -1.0, 0, 0 }, 1.0, sp, ct);
			CHECK(pb.L < 0.05);
			SpinPath pc = spin_path({ -4.0, 0, -3.0 }, { 0, 0, -1 }, { 1, 0, 0 }, 1.0, sp, ct);
			CHECK(std::fabs(pc.L - sp.max_lateral) < 1e-9 && std::fabs(pc.F - sp.bar_forward) < 1e-9);
		}

		{
			const ShotDef &ts = shots[2], &js = shots[8], &cb = shots[9], &fl2 = shots[10], &lo = shots[1];
			CHECK(ts.id == "topspin" && js.id == "jump_serve" && js.topspin_rps > 0.0);
			CHECK(spike_boost_applies(ts, false, kTouchSpike, true, true));
			CHECK(!spike_boost_applies(ts, false, kTouchSpike, true, false));
			CHECK(!spike_boost_applies(ts, false, kTouchSpike, false, true));
			CHECK(!spike_boost_applies(lo, false, kTouchSpike, true, true));
			CHECK(!spike_boost_applies(cb, false, kTouchSpike, true, true) && !spike_boost_applies(fl2, false, kTouchSpike, true, true));
			CHECK(!spike_boost_applies(js, false, kTouchSpike, true, true));
			CHECK(!spike_boost_applies(ts, true, kTouchSpike, true, true));
			CHECK(!spike_boost_applies(ts, false, 2, true, true) && !spike_boost_applies(ts, false, 4, true, true));
			CHECK(!spike_boost_applies(ts, false, 1, true, true) && !spike_boost_applies(ts, false, 5, true, true) && !spike_boost_applies(ts, false, 6, true, true));
			CHECK(yields_to_game_dump(ts, kTouchDump) && !yields_to_game_dump(ts, kTouchSpike) && !yields_to_game_dump(ts, 2));
			CHECK(!yields_to_game_dump(fl2, kTouchDump) && !yields_to_game_dump(cb, kTouchDump));
			CHECK(clamp_spike_boost(1.12) == 1.12 && clamp_spike_boost(0.5) == 1.0 && clamp_spike_boost(5.0) == 1.3 && clamp_spike_boost(NAN) == kSpikeBoostDefault);
			CHECK(kSpikeBoostDefault >= 1.10 && kSpikeBoostDefault <= 1.15);

			const double g = 20.0, r = 0.26, clear = 2.2 + 0.26 + 0.15;
			Vec3 from = { 0.5, 3.1, 1.2 }, tgt = { -0.8, 0.0, -6.0 }, vn = { -2.0, -3.0, -11.0 };
			TopspinSolve a = solve_topspin(from, tgt, vn, g, r, clear, 1.25, 0.1), b = solve_topspin(from, tgt, vn, g, r, clear, 1.25 * 1.12, 0.1);
			const double ha = std::sqrt(a.vel.x * a.vel.x + a.vel.z * a.vel.z), hb = std::sqrt(b.vel.x * b.vel.x + b.vel.z * b.vel.z);
			CHECK(std::fabs(hb / ha - 1.12) < 1e-9 && b.dive_g >= a.dive_g);
			auto land = [&](const TopspinSolve &q) { double gp = q.dive_g * g, vy = q.vel.y, t = (vy + std::sqrt(vy * vy + 2 * gp * (from.y - r))) / gp; return Vec3{ from.x + q.vel.x * t, 0, from.z + q.vel.z * t }; };
			Vec3 la = land(a), lb = land(b);
			CHECK(std::fabs(la.x - tgt.x) < 0.05 && std::fabs(la.z - tgt.z) < 0.05 && std::fabs(lb.x - tgt.x) < 0.05 && std::fabs(lb.z - tgt.z) < 0.05);
		}

		CHECK(id(T(true, false, false, 0.2, { "sprint" })) == "high_serve");
		CHECK(id(T(true, false, false, 0.6, { "sprint" })) == "low_serve");
		CHECK(id(T(true, false, false, 1.0, { "sprint" })) == "fast_serve");
		CHECK(id(T(true, false, true, 0.8, { "sprint" })) == "jump_serve");
		CHECK(id(T(true, false, false, 0.8, { "sprint", "move_back" })) == "standing_float");
		CHECK(id(T(true, false, true, 0.8, { "sprint", "move_back" })) == "float_serve");
		CHECK(id(T(false, false, false, 0.8, { "sprint" })) == "-");

		for (auto &s : shots) {
			std::string c = combo_game_clash(s.combo);
			if (!c.empty()) std::printf("clash %s: %s\n", s.id.c_str(), c.c_str());
			CHECK(c.empty());
			for (auto &h : s.combo.hold) if (h == "move_forward") CHECK(s.id == "topspin" && s.combo.air == "air");
		}
		for (size_t i = 0; i < shots.size(); i++)
			for (size_t j = i + 1; j < shots.size(); j++) {
				bool amb = combos_ambiguous(shots[i].combo, shots[j].combo);
				if (amb) std::printf("ambiguous %s / %s\n", shots[i].id.c_str(), shots[j].id.c_str());
				CHECK(!amb);
			}

		Combo bad; bad.hold = { "move_forward" }; CHECK(!combo_game_clash(bad).empty());
		bad.air = "air"; CHECK(combo_game_clash(bad).empty());
		bad.context = "serve"; CHECK(!combo_game_clash(bad).empty()); bad.context = "rally"; bad.air = "any";
		bad.hold = { "sprint" }; bad.context = "rally"; CHECK(!combo_game_clash(bad).empty());
		bad.hold = { "sprint", "strafe" }; bad.context = "serve"; CHECK(!combo_game_clash(bad).empty());
		bad.hold = { "callout_1" }; CHECK(!combo_game_clash(bad).empty());


		CHECK(camera_curve(+1, { 1, 0, 0 }, { 0, 0, -1 }, 12.0) < -11.9);
		CHECK(camera_curve(-1, { 1, 0, 0 }, { 0, 0, -1 }, 12.0) > 11.9);


		CHECK(camera_curve(+1, { -1, 0, 0 }, { 0, 0, -1 }, 12.0) > 11.9);

		CHECK(camera_curve(+1, { 0, 0, -1 }, { 1, 0, 0 }, 12.0) > 11.9);

		CHECK(camera_curve(+1, { 1, 5, 0 }, { 0, 0, -1 }, 12.0) < -11.9);
		CHECK(camera_curve(0, { 1, 0, 0 }, { 0, 0, -1 }, 12.0) == 0.0);
	}

	CHECK(mode({ ob("g", Context::Lan) }) == "LAN");
	CHECK(mode({ ob("g", Context::CustomLobby, Visibility::Private) }) == "private");
	CHECK(mode({ ob("g", Context::CustomLobby, Visibility::FriendsOnly) }) == "friends-only");
	CHECK(mode({ ob("g", Context::CustomLobby, Visibility::Public) }) == "public");
	CHECK(mode({ ob("g", Context::QuickMatch) }) == "quick match");
	CHECK(mode({ ob("g", Context::ServerBrowser) }) == "server browser");
	CHECK(mode({ off("free_play") }) == "Free Play");
	CHECK(mode({ off("offline_solo") }) == "offline solo");
	CHECK(mode({ off("practice") }) == "practice");
	CHECK(mode({ ob("g", Context::Offline) }) == "menu");
	CHECK(mode({}) == "unknown");
	CHECK(mode({ ob("g", Context::Lan), ob("n", Context::Online, Visibility::Unknown, false) }) == "unknown");
	CHECK(decide({ ob("g", Context::CustomLobby, Visibility::Public) }, 10.0).mode == "public");


	{
		using namespace jvm::pin;
		Pin p; p.name = "steam-26924091"; p.steam_buildid = "26924091"; p.engine = "4.7.1-stable (custom_build)";
		p.pck_size = 171181116; p.pck_sha256 = "5696c7cc027ee370cbaa737a9bb3b4d7d37b48195592f7fb1f24c921c7810f46";
		p.exe_size = 85303808; p.exe_sha256 = "224a5ea56536081b84369a0e90590a688a8510fefcc24ab84af1a31b3e052435";
		Observed o; o.engine = p.engine; o.pck_size = p.pck_size; o.pck_sha256 = "5696C7CC027EE370CBAA737A9BB3B4D7D37B48195592F7FB1F24C921C7810F46";
		o.exe_size = p.exe_size; o.exe_sha256 = p.exe_sha256; o.steam_buildid = "26924091";
		CHECK(match_pins({ p }, o).ok);
		Observed o2 = o; o2.steam_buildid = ""; CHECK(match_pins({ p }, o2).ok);
		Observed sz = o; sz.pck_size = 171181117; CHECK(!match_pins({ p }, sz).ok);
		CHECK(match_pins({ p }, sz).reason.find("pck size") != std::string::npos);
		Observed hs = o; hs.pck_sha256[0] = '0'; CHECK(!match_pins({ p }, hs).ok);
		CHECK(match_pins({ p }, hs).reason.find("SHA256") != std::string::npos);
		Observed en = o; en.engine = "4.7.2-stable (custom_build)"; CHECK(!match_pins({ p }, en).ok);
		Observed ex = o; ex.exe_sha256 = "00"; CHECK(!match_pins({ p }, ex).ok);
		Observed bi = o; bi.steam_buildid = "26999999"; CHECK(!match_pins({ p }, bi).ok);
		Observed miss = o; miss.pck_size = -1; CHECK(!match_pins({ p }, miss).ok);
		CHECK(!match_pins({}, o).ok);
		Pin inc; inc.name = "incomplete"; CHECK(!match_pins({ inc }, o).ok);
		Pin other = p; other.name = "older"; other.pck_size = 1; other.steam_buildid = "1";
		CHECK(match_pins({ other, p }, o).ok && match_pins({ other, p }, o).matched == "steam-26924091");
		CHECK(match_pins({ p }, sz).expected_build == "26924091");
		CHECK(parse_acf_buildid("\"AppState\"\n{\n\t\"appid\"\t\t\"5037200\"\n\t\"buildid\"\t\t\"26924091\"\n}") == "26924091");
		CHECK(parse_acf_buildid("nothing") == "");
	}


	{
		using namespace jvm::skins;
		CHECK(parse_id("A1F3").value() == 0xA1F3);
		CHECK(parse_id("a1f3").value() == 0xA1F3);
		CHECK(parse_id("0000").value() == 0);
		CHECK(!parse_id("A1F"));
		CHECK(!parse_id("A1F34"));
		CHECK(!parse_id("G123"));
		CHECK(!parse_id(""));
		CHECK(format_id(0xA1F3) == "A1F3");
		CHECK(format_id(0x00B) == "000B");
		Registry r = build_registry({ { "alpha", "BEEF" }, { "beta", "beef" }, { "gamma", "12" }, { "delta", "0001" } });
		CHECK(r.by_id.size() == 2 && r.by_id[0xBEEF] == "alpha");
		CHECK(r.warnings.size() == 2);
		Packet e; e.type = Type::EquipOf; e.peer = 123456789; e.name = "Ashton"; e.ids = { 0xA1F3, 0xBEEF };
		auto bytes = encode(e);
		auto d = decode(bytes);
		CHECK(d && d->type == Type::EquipOf && d->peer == 123456789 && d->name == "Ashton" && d->ids == e.ids);
		Packet neg; neg.type = Type::Leave; neg.peer = -5; auto dn = decode(encode(neg)); CHECK(dn && dn->peer == -5);
		Packet h; h.type = Type::Hello; CHECK(decode(encode(h)) && decode(encode(h))->type == Type::Hello);
		Packet q; q.type = Type::Equip; q.ids = { 1, 2, 3 }; CHECK(decode(encode(q))->ids.size() == 3);
		Packet big; big.type = Type::Equip; for (int i = 0; i < 40; i++) big.ids.push_back(i);
		CHECK(decode(encode(big))->ids.size() == kMaxIds);
		Packet ln; ln.type = Type::EquipOf; ln.name = std::string(100, 'x'); CHECK(decode(encode(ln))->name.size() == kMaxName);
		auto bad = bytes; bad[0] = 'X'; CHECK(!decode(bad));
		auto ver = bytes; ver[4] = 9; CHECK(!decode(ver));
		auto trunc = bytes; trunc.pop_back(); CHECK(!decode(trunc));
		auto extra = bytes; extra.push_back(0); CHECK(!decode(extra));
		CHECK(!decode({}));
		CHECK(!decode({ 'J', 'V', 'M', 'S', 1, 99 }));
		std::vector<uint8_t> lie = { 'J', 'V', 'M', 'S', 1, 3, 200 }; CHECK(!decode(lie));
		std::map<SkinId, std::string> inst = { { 0xA1F3, "mine" } };
		std::map<int32_t, RemotePlayer> rem = { { 2, { "Bob", { 0xA1F3, 0xBEEF } } }, { 3, { "", { 0x0C0D } } } };
		auto lines = missing_lines(inst, rem);
		CHECK(lines.size() == 2);
		CHECK(lines[0] == "You're missing BEEF from Bob");
		CHECK(lines[1] == "You're missing 0C0D from player 3");
		CHECK(missing_lines(inst, {}).empty());
	}


	CHECK(visibility_from_string("0") == Visibility::Private);
	CHECK(visibility_from_string("1") == Visibility::FriendsOnly);
	CHECK(visibility_from_string("2") == Visibility::Public);
	CHECK(visibility_from_string("3") == Visibility::Invisible);
	CHECK(visibility_from_string("4") == Visibility::Private);
	CHECK(visibility_from_string("Friends Only") == Visibility::FriendsOnly);
	CHECK(visibility_from_string("PUBLIC") == Visibility::Public);
	CHECK(visibility_from_string("whatever") == Visibility::Unknown);
	CHECK(context_from_string("Quick-Match") == Context::QuickMatch);
	CHECK(context_from_string("custom_lobby") == Context::CustomLobby);
	CHECK(context_from_string("") == Context::Unknown);


	GateStateMachine sm(0.5);
	Decision yes{ true, "y" }, no{ false, "n" };
	CHECK(!sm.active());
	CHECK(!sm.update(yes, 0.0));
	CHECK(!sm.active());
	CHECK(!sm.update(yes, 0.4));
	CHECK(sm.update(yes, 0.5));
	CHECK(sm.active());
	CHECK(sm.update(no, 0.51));
	CHECK(!sm.active());
	CHECK(!sm.update(yes, 0.6));
	CHECK(!sm.update(no, 0.9));
	CHECK(!sm.update(yes, 1.0));
	CHECK(!sm.update(yes, 1.4));
	CHECK(sm.update(yes, 1.5));
	sm.force_off("kill switch");
	CHECK(!sm.active());


	CHECK(is_lan_address("192.168.1.20"));
	CHECK(is_lan_address("10.0.0.5"));
	CHECK(is_lan_address("172.16.0.1"));
	CHECK(is_lan_address("172.31.255.255"));
	CHECK(!is_lan_address("172.32.0.1"));
	CHECK(is_lan_address("127.0.0.1"));
	CHECK(is_lan_address("169.254.3.4"));
	CHECK(!is_lan_address("100.64.0.1"));
	CHECK(!is_lan_address("8.8.8.8"));
	CHECK(!is_lan_address("256.1.1.1"));
	CHECK(!is_lan_address("1.2.3"));
	CHECK(!is_lan_address(""));
	CHECK(is_lan_address("::1"));
	CHECK(is_lan_address("fe80::1"));
	CHECK(is_lan_address("fd12:3456::1"));
	CHECK(!is_lan_address("2001:4860::8888"));
	CHECK(is_lan_address("::ffff:192.168.0.2"));


	auto shots = jvm::shots::default_shots();
	CHECK(shots.size() == 11);
	for (auto &s : shots) {
		auto v = jvm::shots::launch_velocity(s, { 0, 0, -1 });
		double sp = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		CHECK(std::fabs(sp - s.speed) < 1e-9);
		CHECK(v.z < 0);
	}
	auto hs = jvm::shots::launch_velocity(shots[0], { 0, 0, -1 });
	auto ls = jvm::shots::launch_velocity(shots[1], { 0, 0, -1 });
	CHECK(hs.y > ls.y);
	auto fl = shots[4];
	auto a0 = jvm::shots::float_accel(fl, { 1, 0, 0 }, 0.3);
	auto aEnd = jvm::shots::float_accel(fl, { 1, 0, 0 }, 99.0);
	CHECK(std::fabs(a0.z) > 0.0 && aEnd.z == 0.0);


	{
		using namespace jvm::shots;
		for (double yaw_deg : { 0.0, 37.0, 90.0, 180.0, -135.0 }) {
			double y = yaw_deg * 3.14159265358979 / 180.0;

			double pc = std::cos(0.5236), ps = std::sin(0.5236);
			Vec3 cx = { std::cos(y), 0, -std::sin(y) }, cz = { std::sin(y) * pc, ps, std::cos(y) * pc };
			Vec3 fwd = camera_relative_move(cx, cz, 0, -1);
			Vec3 right = camera_relative_move(cx, cz, 1, 0);
			Vec3 back = camera_relative_move(cx, cz, 0, 1);
			CHECK(std::fabs(fwd.x + std::sin(y)) < 1e-6 && std::fabs(fwd.z + std::cos(y)) < 1e-6);
			CHECK(std::fabs(right.x - std::cos(y)) < 1e-6 && std::fabs(right.z + std::sin(y)) < 1e-6);
			CHECK(std::fabs(back.x + fwd.x) < 1e-9 && std::fabs(back.z + fwd.z) < 1e-9);

			CHECK(world_curve(right, fwd, 12.0) < -11.9);
			CHECK(world_curve(camera_relative_move(cx, cz, -1, 0), fwd, 12.0) > 11.9);

			CHECK(world_curve(right, back, 12.0) > 11.9);
		}
		auto mi = move_intent(0, -1); CHECK(mi.forward && mi.side == 0);
		mi = move_intent(0.7, -0.7); CHECK(mi.forward && mi.side == 1);
		mi = move_intent(-1, 0); CHECK(!mi.forward && mi.side == -1);
		mi = move_intent(0, 1); CHECK(!mi.forward && mi.side == 0 && mi.back);
		mi = move_intent(-0.6, 0.8); CHECK(mi.back && mi.side == -1);
		mi = move_intent(-0.8, 0.6); CHECK(!mi.back && mi.side == -1);
		mi = move_intent(0, -1); CHECK(!mi.back);
		mi = move_intent(0.3, -0.3); CHECK(!mi.forward && mi.side == 0);

		auto sh = default_shots();
		auto pick = [&](bool serving, bool air, std::vector<std::string> held) {
			TouchState t; t.serving = serving; t.airborne = air; t.power = 0.5; t.held = held;
			int i = select_shot(sh, t); return i < 0 ? std::string("-") : sh[i].id;
		};
		CHECK(pick(false, true, { "side" }) == "curveball");
		CHECK(pick(false, false, { "side" }) == "-");
		CHECK(pick(true, true, { "side" }) == "-");
		CHECK(pick(false, true, { "move_forward" }) == "topspin");
		CHECK(pick(false, true, { "move_back" }) == "-");
		CHECK(pick(false, true, { "back" }) == "float");
		CHECK(pick(false, false, { "back" }) == "-");
		CHECK(pick(true, true, { "back" }) == "-");
		{ TouchState t; t.airborne = true; t.set_touch = true; t.power = 0.5; t.held = { "back" }; CHECK(select_shot(sh, t) < 0); }
		CHECK(!combo_game_clash(C2("touch", { "back" }, "rally", "any", "ground")).empty());
		CHECK(!combo_game_clash(C2("touch", { "move_back" }, "rally", "any", "air")).empty());
		CHECK(combo_game_clash(sh[10].combo).empty());
		CHECK(pick(false, false, { "move_back" }) == "-");
		CHECK(pick(false, false, { "move_forward" }) == "-");
		CHECK(pick(true, true, { "sprint", "move_back" }) == "float_serve");
		for (size_t i = 0; i < sh.size(); i++) for (size_t j = i + 1; j < sh.size(); j++) CHECK(!combos_ambiguous(sh[i].combo, sh[j].combo));
		CHECK(combo_game_clash(sh[9].combo).empty());
	}

	{
		using namespace jvm::shots;
		SpinParams p; Court c;
		Vec3 aim = { 0, 0, -5 }, right = { 1, 0, 0 };
		double prev = -1;
		for (double ch : { 0.0, 0.05, 0.3, 0.6, 0.9, 1.0 }) {
			Vec3 t = spin_target(aim, right, ch, p, c);
			CHECK(t.x >= prev - 1e-12); prev = t.x;
		}
		CHECK(std::fabs(spin_target(aim, right, 0.05, p, c).x - 0.3) < 1e-9);
		CHECK(std::fabs(spin_target(aim, right, 1.0, p, c).x - 4.5) < 1e-9);
		Vec3 down = { 0, 0, -1 };
		CHECK(std::fabs(spin_target(aim, down, 1.0, p, c).z + 9.0) < 1e-9);
		{
			const double g = 20.0, r = 0.26, clear = 2.2 + 0.26 + 0.15;
			auto sim = [&](Vec3 p0, const TopspinSolve &ts, double &apex, double &net_y) {
				Vec3 p = p0, v = ts.vel; apex = p.y; net_y = 99;
				for (int i = 0; i < 2000; i++) {
					Vec3 q = p; v.y -= g * ts.dive_g / 60.0; p = { p.x + v.x / 60.0, p.y + v.y / 60.0, p.z + v.z / 60.0 };
					apex = std::fmax(apex, p.y);
					if ((q.z > 0) != (p.z > 0)) { double f = q.z / (q.z - p.z); net_y = q.y + (p.y - q.y) * f; }
					if (q.y > r + 0.001 && p.y <= r + 0.02) return p;
				}
				return p;
			};
			for (Vec3 from : { Vec3{ 0, 3.6, 7.0 }, Vec3{ 1, 3.6, 4.5 }, Vec3{ -1, 3.4, 1.8 } })
				for (Vec3 tgt : { Vec3{ 0, 0, -7.5 }, Vec3{ 0, 0, -2.0 }, Vec3{ -3, 0, -4.5 }, Vec3{ 3, 0, -4.5 } }) {
					Vec3 vn = { (tgt.x - from.x) * 0.8, -1.0, (tgt.z - from.z) * 0.8 };
					TopspinSolve ts = solve_topspin(from, tgt, vn, g, r, clear, 1.25, 0.4);
					double apex, ny; Vec3 l = sim(from, ts, apex, ny);
					double err = std::sqrt((l.x - tgt.x) * (l.x - tgt.x) + (l.z - tgt.z) * (l.z - tgt.z));
					double vh = std::sqrt(ts.vel.x * ts.vel.x + ts.vel.z * ts.vel.z), vhn = std::sqrt(vn.x * vn.x + vn.z * vn.z);
					CHECK(err < 0.35);
					CHECK(ts.dive_g >= 1.0 && vh >= vhn * 1.25 - 1e-9);
					CHECK(ny >= clear - 0.05);
					CHECK(apex <= from.y + 0.4 + 4.0 + 0.1);
				}
		}
		{
			for (double deg : { 0.0, 90.0, 200.0 }) {
				double r = deg * 3.14159265358979 / 180.0;
				Vec3 fwd = { std::sin(r), 0, -std::cos(r) };
				Vec3 rightv = { -fwd.z, 0, fwd.x };
				Vec3 con = { 1, 3.5, 6 }, am = { con.x + fwd.x * 8, 0, con.z + fwd.z * 8 };
				for (int sd : { -1, 1 }) {
					Vec3 t = float_side_target(con, am, fwd, sd, 3.0);
					double lat = (t.x - am.x) * rightv.x + (t.z - am.z) * rightv.z;
					double rng = std::sqrt((t.x - con.x) * (t.x - con.x) + (t.z - con.z) * (t.z - con.z));
					CHECK(lat * sd > 2.5 && std::fabs(rng - 8.0) < 1e-9);
				}
				Vec3 st = float_side_target(con, am, fwd, 0, 3.0); CHECK(st.x == am.x && st.z == am.z);
			}
			Vec3 fwd = { 0, 0, -1 };
			CHECK(float_side_target({ 0, 3, 6 }, { 0, 0, -2 }, fwd, 1, 3.0).x > 2.5 && float_side_target({ 0, 3, 6 }, { 0, 0, -2 }, fwd, -1, 3.0).x < -2.5);
			Vec3 side_on = { -1, 0, 0 };
			CHECK(float_side_target({ 0, 3, 6 }, { 0, 0, -2 }, side_on, 1, 3.0).x > 2.5 && float_side_target({ 0, 3, 6 }, { 0, 0, -2 }, side_on, -1, 3.0).x < -2.5);
		}
		{
			auto cam = [](Vec3 look) { Vec3 cz = { -look.x, 0, -look.z }; Vec3 cx = { -look.z, 0, look.x }; return std::make_pair(cx, cz); };
			auto intent = [&](Vec3 look, Vec3 net, double rx, double ry) { auto c = cam(look); FrameRaw f = hitter_frame_raw(c.first, c.second, rx, ry, net); return move_intent(f.x, f.y); };
			Vec3 net_a = { 0, 0, -1 }, net_b = { 0, 0, 1 };

			CHECK(intent({ 0, 0, -1 }, net_a, 0, -1).forward); CHECK(intent({ 0, 0, -1 }, net_a, 0, 1).back); CHECK(intent({ 0, 0, -1 }, net_a, 1, 0).side == 1);

			CHECK(intent({ -1, 0, 0 }, net_a, 1, 0).forward); CHECK(!intent({ -1, 0, 0 }, net_a, 0, -1).forward);
			CHECK(intent({ -1, 0, 0 }, net_a, 0, -1).side == -1); CHECK(intent({ -1, 0, 0 }, net_a, -1, 0).back);

			CHECK(intent({ 1, 0, 0 }, net_a, -1, 0).forward); CHECK(intent({ 1, 0, 0 }, net_a, 1, 0).back); CHECK(intent({ 1, 0, 0 }, net_a, 0, -1).side == 1);

			CHECK(intent({ 0, 0, 1 }, net_b, 0, -1).forward); CHECK(intent({ 0, 0, 1 }, net_b, 1, 0).side == 1);

			CHECK(intent({ 0, 0, 1 }, net_a, 0, -1).back);

			Vec3 nd = hitter_net_dir({ 0, 0, 6 }, { 3, 0, -2 }, 1); CHECK(std::fabs(nd.x - 3 / std::sqrt(73.0)) < 1e-9);
			nd = hitter_net_dir({ 0, 0, 6 }, { 0.2, 0, 6.3 }, 1); CHECK(nd.z == -1.0);
			nd = hitter_net_dir({ 0, 0, -6 }, { 0.2, 0, -6.3 }, 0); CHECK(nd.z == 1.0);

			nd = hitter_net_dir({ 3.9, 0, 3.5 }, { 0.5, 0, 4.0 }, 1); CHECK(nd.z == -1.0 && nd.x == 0.0);
			{ FrameRaw f = hitter_frame_raw({ 1, 0, 0 }, { 0, 0, 1 }, 1.0, 0.0, nd); CHECK(f.x > 0.99 && std::fabs(f.y) < 1e-9); }
		}
		{
			for (double deg : { 30.0, 60.0, 90.0, 135.0 }) {
				double r = deg * 3.14159265358979 / 180.0;
				Vec3 d = { std::cos(r), 0, std::sin(r) };
				for (double ch : { 0.3, 1.0 }) {
					Vec3 t = spin_target(aim, d, ch, p, c);
					double ox = t.x - aim.x, oz = t.z - aim.z, L = std::sqrt(ox * ox + oz * oz);
					CHECK(L > 0.1 && std::fabs(ox / L - d.x) < 1e-9 && std::fabs(oz / L - d.z) < 1e-9);
					CHECK(t.z <= -0.3 + 1e-9 && t.z >= -9.0 - 1e-9 && std::fabs(t.x) <= 4.5 + 1e-9);
				}
			}
		}

		CHECK(std::fabs(fhash(12345, 0) - 0.08123189400938893) < 1e-15);
		CHECK(std::fabs(fhash(999983, 3) - 0.544910489835269) < 1e-15);
		CHECK(std::fabs(fhash(7, 10) - 0.7293425429283373) < 1e-15);

		double sl = 0, ss = 0; int n = 4000;
		Vec3 t0 = spin_target(aim, right, 0.5, p, c);
		for (int k = 1; k <= n; k++) {
			Vec3 l = spin_landing(t0, right, 1.0, p, k * 7919LL);
			sl += (l.x - t0.x) * (l.x - t0.x); ss += (l.z - t0.z) * (l.z - t0.z);
		}
		CHECK(sl / n > 4.0 * ss / n);
		Vec3 a = spin_landing(t0, right, 1.0, p, 12345), b = spin_landing(t0, right, 1.0, p, 12345);
		CHECK(a.x == b.x && a.z == b.z);
		Vec3 a2 = spin_landing(t0, right, 1.0, p, 12346);
		CHECK(a2.x != a.x);

		CHECK(spin_spread_scale(1.0, 0.5, p) < spin_spread_scale(0.4, 0.5, p));
		CHECK(spin_spread_scale(1.0, 0.5, p) == 1.0);
		CHECK(spin_spread_scale(1.0, 1.0, p) > spin_spread_scale(1.0, 0.5, p));
		CHECK(spin_spread_scale(1.0, 0.0, p) > spin_spread_scale(1.0, 0.5, p));

		bool out = false;
		Vec3 tf = spin_target(aim, right, 1.0, p, c);
		for (int k = 1; k < 200 && !out; k++) out = spin_landing(tf, right, spin_spread_scale(0.3, 1.0, p), p, k).x > c.half_w;
		CHECK(out);
	}
	{
		using namespace jvm::models;
		RawManifest m; m.kind = "model"; m.id = "ae01"; m.name = "Aero"; m.model = "aero_player.glb";
		ParseResult r = parse_manifest(m);
		CHECK(r.spec && r.errors.empty());
		CHECK(r.spec->id == 0xAE01 && r.spec->ext == "glb" && r.spec->model == "aero_player.glb");
		CHECK(r.spec->hide_base && r.spec->tint == Tint::None && r.spec->hands == Hands::Keep && r.spec->target == Target::Local);
		CHECK(!r.spec->allow_scripts && r.spec->flash && r.spec->tint_param == "albedo" && r.spec->tint_strength == 1.0);
		CHECK(is_model_kind("Model") && !is_model_kind("skin") && !is_model_kind("global"));
		RawManifest k = m; k.kind = "skin"; CHECK(!parse_manifest(k).spec);
		RawManifest t = m; t.model = "models\\aero.TSCN"; r = parse_manifest(t);
		CHECK(r.spec && r.spec->model == "models/aero.TSCN" && r.spec->ext == "tscn");
		for (const char *bad : { "../x.glb", "a/../../x.glb", "/abs/x.glb", "C:/x.glb", "res://x.glb", "user://x.glb", "", "./" }) {
			RawManifest b = m; b.model = bad; CHECK(!parse_manifest(b).spec);
		}
		RawManifest e = m; e.model = "aero.fbx"; CHECK(!parse_manifest(e).spec);
		RawManifest i = m; i.id = "XYZ1"; CHECK(!parse_manifest(i).spec);
		CHECK(safe_relpath("./a/./b.glb") == std::optional<std::string>("a/b.glb"));
		RawManifest o = m; o.team_tint = "skin"; o.game_hands = "tint"; o.apply_to = "all"; o.allow_scripts = true; o.hide_base = false;
		o.tint_strength = 3.0; o.tint_light["sss_tint"] = 0.42; o.tint_param = "body_col"; o.flash = false;
		r = parse_manifest(o);
		CHECK(r.spec && r.spec->tint == Tint::None && r.spec->hands == Hands::Tint && r.spec->target == Target::All && r.spec->allow_scripts && !r.spec->hide_base);
		CHECK(r.spec->tint_strength == 1.0 && r.spec->tint_light.at("sss_tint") == 0.42 && r.spec->tint_param == "body_col" && !r.spec->flash);
		CHECK(r.warnings.size() == 2);
		for (const char *tt : { "jersey", "team", "skin" }) { RawManifest j = m; j.team_tint = tt; r = parse_manifest(j); CHECK(r.spec && r.spec->tint == Tint::None && r.warnings.size() == 1 && r.warnings[0].find("team tint is disabled") != std::string::npos); }
		RawManifest nt = m; nt.team_tint = "none"; r = parse_manifest(nt); CHECK(r.spec && r.spec->tint == Tint::None && r.warnings.empty());
		RawManifest w = m; w.team_tint = "rainbow"; w.game_hands = "juggle"; w.apply_to = "mars"; w.bad_types.push_back("hide_base");
		r = parse_manifest(w);
		CHECK(r.spec && r.warnings.size() == 4 && r.spec->tint == Tint::None && r.spec->hands == Hands::Keep && r.spec->target == Target::Local);
		RawManifest c = m; c.color = "#FF1A8C"; r = parse_manifest(c);
		CHECK(r.spec && r.spec->has_color && r.spec->tint == Tint::None && std::fabs(r.spec->color[0] - 1.0f) < 1e-6 && std::fabs(r.spec->color[1] - 26 / 255.0f) < 1e-6);
		RawManifest c2 = m; c2.color = "pink"; r = parse_manifest(c2); CHECK(r.spec && !r.spec->has_color && r.warnings.size() == 1);
		{
			CHECK(!r.spec->arms.follow);
			RawManifest a = m; a.has_arms = true; r = parse_manifest(a);
			CHECK(r.spec && r.spec->arms.follow && r.spec->arms.anchor_bone == "spine1" && r.spec->arms.upper == 0.45 && r.warnings.empty());
			a.arms_mode = "model"; r = parse_manifest(a); CHECK(r.spec && !r.spec->arms.follow && r.warnings.empty());
			a.arms_mode = "wiggle"; r = parse_manifest(a); CHECK(r.spec && !r.spec->arms.follow && r.warnings.size() == 1);
			a.arms_mode = "follow_hands"; a.arms_upper = 0.5; a.arms_lower = -1.0; a.arms_shoulder = std::vector<double>{ 0.6, 2.4, 0.1 };
			a.arms_radius = std::vector<double>{ 0.1, 0.0, 0.1 }; a.arms_elbow_hint = std::vector<double>{ 1, 0 }; a.arms_anchor_bone = "chest";
			r = parse_manifest(a);
			CHECK(r.spec && r.spec->arms.follow && r.spec->arms.upper == 0.5 && r.spec->arms.lower == 0.72 && r.spec->arms.shoulder[0] == 0.6 && r.spec->arms.shoulder[2] == 0.1);
			CHECK(r.spec->arms.radius[1] == 0.16 && r.spec->arms.elbow_hint[1] == -0.5 && r.spec->arms.anchor_bone == "chest" && r.warnings.size() == 3);
			auto dist = [](V3 p, V3 q) { return std::sqrt((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z)); };
			V3 sh{ 0.5, 2.42, 0 }, hand{ 0.865, 1.381, 0 }, hint{ 1, -0.5, -0.6 };
			V3 e = solve_elbow(sh, hand, 0.45, 0.72, hint);
			CHECK(std::fabs(dist(sh, e) - 0.45) < 1e-9 && std::fabs(dist(e, hand) - 0.72) < 1e-9);
			CHECK(e.z < 0);
			V3 far{ 0.5, 0.5, 0 }; e = solve_elbow(sh, far, 0.45, 0.72, hint);
			CHECK(std::fabs(e.x - 0.5) < 1e-9 && std::fabs(e.y - (2.42 - 1.92 * 0.45 / 1.17)) < 1e-9);
			V3 near{ 0.5, 2.3, 0 }; e = solve_elbow(sh, near, 0.45, 0.72, hint);
			CHECK(std::isfinite(e.x) && std::isfinite(e.y) && std::isfinite(e.z) && std::fabs(dist(sh, e) - 0.45) < 1e-6);
			e = solve_elbow(sh, sh, 0.45, 0.72, hint); CHECK(std::isfinite(e.y));
			V3 up{ 0.5, 3.5, 0 }; e = solve_elbow(sh, up, 0.45, 0.72, V3{ 0, 1, 0 }); CHECK(std::isfinite(e.x) && std::fabs(dist(sh, e) - 0.45) < 1e-6);
		}
		float col[4]; CHECK(parse_color("fff", col) && col[0] == 1.0f && parse_color("#00000080", col) && std::fabs(col[3] - 128 / 255.0f) < 1e-6 && !parse_color("#12345", col));
		{
			Spec us = *parse_manifest(m).spec; std::vector<std::string> uw;
			UserSettings u; u.color = "#d80836"; u.game_hands = "hide"; u.apply_to = "all";
			apply_user_settings(us, u, uw);
			CHECK(uw.empty() && us.has_color && us.user_color && us.hands == Hands::Hide && us.target == Target::All && us.tint == Tint::None);
			CHECK(std::fabs(us.color[0] - 0xD8 / 255.0f) < 1e-6 && std::fabs(us.color[2] - 0x36 / 255.0f) < 1e-6 && us.color[3] == 1.0f);
			CHECK(format_color(us.color) == "#D80836");
			Spec ub = *parse_manifest(c).spec; uw.clear();
			UserSettings u2; u2.color = "#2F6BFF"; apply_user_settings(ub, u2, uw); CHECK(ub.user_color && format_color(ub.color) == "#2F6BFF");
			Spec un = *parse_manifest(c).spec; uw.clear(); apply_user_settings(un, UserSettings{}, uw);
			CHECK(uw.empty() && un.has_color && !un.user_color && format_color(un.color) == "#FF1A8C");
			Spec ux = *parse_manifest(m).spec; uw.clear(); UserSettings u3; u3.color = "red"; u3.game_hands = "wave"; u3.apply_to = "moon";
			apply_user_settings(ux, u3, uw); CHECK(uw.size() == 3 && !ux.has_color && ux.hands == Hands::Keep && ux.target == Target::Local);
		}

		std::map<SkinId, InstalledPack> inst = { { 0xAE01, { true, true, "aero" } }, { 0xCAFE, { false, true, "cafe" } }, { 0xBAD1, { true, false, "broken" } } };
		EquipResult q = select_model(std::string("AE01"), { "CAFE" }, inst);
		CHECK(q.model && *q.model == 0xAE01 && q.warnings.empty());
		CHECK(!select_model(std::nullopt, { "CAFE" }, inst).model);
		CHECK(!select_model(std::string(""), {}, inst).model);
		q = select_model(std::nullopt, { "AE01" }, inst);
		CHECK(!q.model && q.warnings.size() == 1 && q.warnings[0].find("put it in \"model\"") != std::string::npos);
		q = select_model(std::string("CAFE"), {}, inst); CHECK(!q.model && q.warnings[0].find("not a model pack") != std::string::npos);
		q = select_model(std::string("BAD1"), {}, inst); CHECK(!q.model && q.warnings[0].find("invalid skin.json") != std::string::npos);
		q = select_model(std::string("0000"), {}, inst); CHECK(!q.model && q.warnings[0].find("not installed") != std::string::npos);
		q = select_model(std::string("nope"), {}, inst); CHECK(!q.model && q.warnings.size() == 1);

		std::vector<std::string> base = { "root", "spine", "spine1", "head", "hand.l", "hand.r", "forearm.r" };
		BoneCheck bc = check_bones({ "root", "head", "hand.l" }, base); CHECK(bc.ok && bc.resolved.at("hand.l") == "hand.l");
		bc = check_bones({ "hand_l", "Hand.R", "FOREARM-R" }, base);
		CHECK(bc.ok && bc.resolved.at("hand_l") == "hand.l" && bc.resolved.at("Hand.R") == "hand.r" && bc.resolved.at("FOREARM-R") == "forearm.r");
		bc = check_bones({ "head", "lowerarm.R", "tail1", "tail1", "" }, base);
		CHECK(!bc.ok && bc.missing.size() == 3 && bc.missing[0] == "lowerarm.R" && bc.missing[2] == "<unnamed>");
		CHECK(join({ "a", "b", "c" }, ", ", 2) == "a, b, ... (+1)");

		CHECK(bytes_have_script("[ext_resource type=\"Script\" path=\"scripts/x.gd\" id=\"1\"]"));
		CHECK(bytes_have_script("[sub_resource type=\"GDScript\" id=\"g\"]"));
		CHECK(!bytes_have_script("[ext_resource type=\"Shader\" path=\"shaders/aero_bubble.gdshader\" id=\"1\"]"));
		CHECK(is_script_path("res://a/b.gd") && is_script_path("uid://x::Script::res://a.gd") && is_script_path("C:/m/x.CS") && !is_script_path("shaders/a.gdshader"));
	}
	{
		using namespace jvm::shots;
		SpinParams sp; Court ct;
		CHECK(sp.even_bend);
		sp.even_bend = false;
		CHECK(std::fabs(spin_k(0.0, sp) - sp.k_min) < 1e-12 && std::fabs(spin_k(1.0, sp) - sp.k_max) < 1e-12 && std::fabs(spin_k(7.0, sp) - sp.k_max) < 1e-12);
		for (double k : { 0.0, 0.3, 2.0, 4.0 }) {
			CHECK(std::fabs(spin_g(k, 0.0)) < 1e-12 && std::fabs(spin_g(k, 1.0) - 1.0) < 1e-12);
			double prev = -1; bool mono = true; for (int i = 0; i <= 20; i++) { double g = spin_g(k, i / 20.0); mono = mono && g >= prev; prev = g; } CHECK(mono);
		}

		CHECK(spin_g(4.0, 0.5) < spin_g(2.0, 0.5) && spin_g(2.0, 0.5) < spin_g(0.3, 0.5) && std::fabs(spin_g(0.0, 0.5) - 0.5) < 1e-12);
		CHECK(spin_g(4.0, 0.5) < 0.12);
		CHECK(std::fabs(spin_g(0.3, 0.5) - 0.5) < 0.05);

		Vec3 aim = { 0, 0, -4 }, fwd = { 0, 0, -1 }, d = { 1, 0, 0 };
		SpinPath P = spin_path(aim, fwd, d, 1.0, sp, ct);
		Vec3 a0 = P.at(0), a1 = P.at(1);
		CHECK(std::fabs(a0.x) < 1e-12 && std::fabs(a0.z + 4) < 1e-12);
		CHECK(std::fabs(a1.x - P.L) < 1e-9 && std::fabs(a1.z - (-4 - P.F)) < 1e-9);
		CHECK(P.L > 4.0 && P.L <= sp.max_lateral + 1e-9);
		for (int i = 0; i <= 24; i++) { Vec3 a = P.at(i / 24.0); CHECK(a.x <= 4.5 + 1e-6 && a.z >= -9 - 1e-6 && a.z <= -0.3 + 1e-6); }
		Vec3 t0 = P.tangent(0), t1 = P.tangent(1);
		CHECK(-t0.z > 0.8 && t1.x > 0.95);
		SpinPath Pt = spin_path(aim, fwd, d, 0.0, sp, ct);
		CHECK(Pt.at(0.0).x == 0.0 && std::fabs(Pt.k - sp.k_min) < 1e-12);

		std::vector<BarShard> bs = spin_bar_shards(P, 0.5, 0.9);
		CHECK(bs.size() >= 8 && bs.size() % 2 == 0);
		bool on_edge = true, inward = true;
		for (size_t i = 0; i < bs.size(); i++) {
			double best = 1e9; Vec3 near_p;
			for (int j = 0; j <= 400; j++) { Vec3 a = P.at(j / 400.0); double dd = std::hypot(a.x - bs[i].tip.x, a.z - bs[i].tip.z); if (dd < best) { best = dd; near_p = a; } }
			on_edge = on_edge && std::fabs(best - 0.5) < 0.05;
			inward = inward && (near_p.x - bs[i].tip.x) * bs[i].point.x + (near_p.z - bs[i].tip.z) * bs[i].point.z > 0.0;
		}
		CHECK(on_edge && inward);
		CHECK(std::fabs(std::hypot(bs[0].point.x, bs[0].point.z) - 1.0) < 1e-9 && std::fabs(bs[0].point.x * P.tangent(0).x + bs[0].point.z * P.tangent(0).z - 0.7071) < 0.01);

		CHECK(std::fabs(ramp_curve(2.0, 4.0, 1.0, 0.0) - 2.0) < 1e-12 && std::fabs(ramp_curve(2.0, 4.0, 1.0, 1.0) - 2.0 * std::exp(4.0)) < 1e-9 && std::fabs(ramp_curve(2.0, 4.0, 1.0, 9.0) - 2.0 * std::exp(4.0)) < 1e-9);


		const double g = 11.0, dt = 1.0 / 60.0, ref = 13.0, r = 0.13;
		Vec3 from = { 0, 2.8, 5 };
		auto launch = [&](Vec3 at) { double dx = at.x - from.x, dz = at.z - from.z, dist = std::hypot(dx, dz), hs = 12.0, T = dist / hs; double vy = ((r - from.y) + 0.5 * g * T * T) / T; return Vec3{ dx / dist * hs, vy, dz / dist * hs }; };
		auto flight_T = [&](Vec3 at) { return std::hypot(at.x - from.x, at.z - from.z) / 12.0; };
		std::vector<Vec3> track;
		auto fly = [&](Vec3 at, double A, double k, bool keep) {
			Vec3 p = from, v = launch(at); double T = flight_T(at), t = 0;
			if (keep) track.clear();
			for (int i = 0; i < 400; i++) {
				double c = ramp_curve(A, k, T, t);
				double sp2 = std::hypot(v.x, v.z);
				v.y -= g * dt;
				if (sp2 > 1.0) { Vec3 fl = { v.x / sp2, 0, v.z / sp2 }; Vec3 side = { fl.z, 0, -fl.x };  v.x += side.x * c * (sp2 / ref) * dt; v.z += side.z * c * (sp2 / ref) * dt; }
				Vec3 b = p; p = { p.x + v.x * dt, p.y + v.y * dt, p.z + v.z * dt }; t += dt;
				if (keep) track.push_back(p);
				if (b.y > r && p.y <= r) break;
			}
			return p;
		};
		for (double ch : { 0.15, 0.6, 1.0 }) {
			SpinPath Q = spin_path(aim, { aim.x - from.x, 0, aim.z - from.z }, d, ch, sp, ct);
			Vec3 want = Q.at(ch);
			double k = Q.k;

			RampSolve rs = solve_ramp([&](Vec3 at, double A) { return fly(at, -A, k, false); }, want, d, Q.lateral(ch));
			if (rs.err >= 0.1) std::printf("ramp solve ch %.2f err %.3f A %.2f iters %d defl %.4f\n", ch, rs.err, rs.A, rs.iters, rs.defl_per_A);
			CHECK(rs.err < 0.1);
			CHECK(ch < 0.2 ? std::fabs(rs.A) < 25.0 : rs.A > 0.0);


			fly(rs.aim, -rs.A, k, true); std::vector<Vec3> bent = track;
			fly(rs.aim, 0.0, k, true); std::vector<Vec3> straight = track;
			size_t n = std::min(bent.size(), straight.size());
			double dev_end = bent[n - 1].x - straight[n - 1].x, dev_mid = bent[n / 2].x - straight[n / 2].x;
			if (ch >= 0.99) CHECK(dev_end > 1.0 && dev_mid < 0.2 * dev_end);
		}
	}
	{

		using namespace jvm::shots;
		SpinParams sp; Court ct;
		Vec3 aim = { 0, 0, -4 }, fwd = { 0, 0, -1 }, d = { 1, 0, 0 };
		for (double ch : { 0.0, 0.05, 0.25, 0.38, 0.5, 0.69, 0.9, 1.0 }) {
			SpinPath P = spin_path(aim, fwd, d, ch, sp, ct);
			CHECK(P.even && P.k_time() == 0.0);
			CHECK(std::fabs(P.s_for(ch) - std::sqrt(ch)) < 1e-12);
			CHECK(std::fabs(P.lateral(P.s_for(ch)) - P.L * ch) < 1e-9);
			Vec3 w = P.at(P.s_for(ch));
			CHECK(std::fabs(w.x - P.L * ch) < 1e-9 && std::fabs(w.z - (-4 - P.F * std::sqrt(ch))) < 1e-9);
			for (int i = 0; i <= 24; i++) { Vec3 a = P.at(i / 24.0); CHECK(a.x <= 4.5 + 1e-6 && a.z >= -9 - 1e-6 && a.z <= -0.3 + 1e-6); }
		}
		SpinPath P = spin_path(aim, fwd, d, 1.0, sp, ct);
		CHECK(std::fabs(P.g(0.5) - 0.25) < 1e-12 && std::fabs(P.g(1.0) - 1.0) < 1e-12 && std::fabs(P.g_slope(0.5) - 1.0) < 1e-12);
		Vec3 a1 = P.at(1); CHECK(std::fabs(a1.x - P.L) < 1e-9 && std::fabs(a1.z - (-4 - P.F)) < 1e-9);
		Vec3 t0 = P.tangent(0); CHECK(-t0.z > 0.99);

		CHECK(std::fabs(P.g_slope(0.5) - 2.0 * P.g_slope(0.25)) < 1e-12 && std::fabs(P.g_slope(1.0) - 2.0 * P.g_slope(0.5)) < 1e-12);
		std::vector<BarShard> bs = spin_bar_shards(P, 0.5, 0.9);
		bool on_edge = true;
		for (auto &b : bs) { double best = 1e9; for (int j = 0; j <= 400; j++) { Vec3 a = P.at(j / 400.0); best = std::fmin(best, std::hypot(a.x - b.tip.x, a.z - b.tip.z)); } on_edge = on_edge && best <= 0.5 + 0.05; }
		CHECK(bs.size() >= 8 && on_edge);
		CHECK(std::fabs(ramp_curve(2.0, 0.0, 1.0, 0.0) - 2.0) < 1e-12 && std::fabs(ramp_curve(2.0, 0.0, 1.0, 0.7) - 2.0) < 1e-12);


		const double g = 11.0, dt = 1.0 / 60.0, ref = 13.0, r = 0.13;
		Vec3 from = { 0, 2.8, 5 };
		auto launch = [&](Vec3 at) { double dx = at.x - from.x, dz = at.z - from.z, dist = std::hypot(dx, dz), hs = 12.0, T = dist / hs; double vy = ((r - from.y) + 0.5 * g * T * T) / T; return Vec3{ dx / dist * hs, vy, dz / dist * hs }; };
		std::vector<Vec3> track;
		auto fly = [&](Vec3 at, double A, bool keep) {
			Vec3 p = from, v = launch(at);
			if (keep) track.clear();
			for (int i = 0; i < 400; i++) {
				double sp2 = std::hypot(v.x, v.z);
				v.y -= g * dt;
				if (sp2 > 1.0) { Vec3 fl = { v.x / sp2, 0, v.z / sp2 }; Vec3 side = { fl.z, 0, -fl.x }; v.x += side.x * A * (sp2 / ref) * dt; v.z += side.z * A * (sp2 / ref) * dt; }
				Vec3 b = p; p = { p.x + v.x * dt, p.y + v.y * dt, p.z + v.z * dt };
				if (keep) track.push_back(p);
				if (b.y > r && p.y <= r) break;
			}
			return p;
		};
		double lat_prev = -1;
		for (double ch : { 0.15, 0.38, 0.6, 1.0 }) {
			SpinPath Q = spin_path(aim, { aim.x - from.x, 0, aim.z - from.z }, d, ch, sp, ct);
			double s = Q.s_for(ch);
			Vec3 want = Q.at(s);
			RampSolve rs = solve_ramp([&](Vec3 at, double A) { return fly(at, -A, false); }, want, d, Q.lateral(s));
			if (rs.err >= 0.1) std::printf("even solve ch %.2f err %.3f A %.2f iters %d\n", ch, rs.err, rs.A, rs.iters);
			CHECK(rs.err < 0.1);
			CHECK(rs.A > 0.0);
			CHECK(Q.lateral(s) > lat_prev); lat_prev = Q.lateral(s);
			fly(rs.aim, -rs.A, true); std::vector<Vec3> bent = track;
			fly(rs.aim, 0.0, true); std::vector<Vec3> straight = track;
			size_t n = std::min(bent.size(), straight.size());
			double dev_end = bent[n - 1].x - straight[n - 1].x, dev_mid = bent[n / 2].x - straight[n / 2].x;
			CHECK(dev_end > 0.0 && dev_mid > 0.15 * dev_end && dev_mid < 0.4 * dev_end);
		}
	}


	{
		using namespace jvm::acc;
		CHECK(slot_name("head", 0) == "head");
		CHECK(slot_name("head", 1) == "head2");
		CHECK(slot_name("extra", 2) == "extra3");
		CHECK(category_of_slot("head") == "head");
		CHECK(category_of_slot("head2") == "head");
		CHECK(category_of_slot("extra3") == "extra");
		CHECK(category_of_slot("face") == "face");
		CHECK(locker_category(true, false, false, "", "skin") == "head");
		CHECK(locker_category(false, false, true, "", "skin") == "extra");
		CHECK(locker_category(false, true, false, "", "skin") == "face");
		CHECK(locker_category(false, false, false, "", "face") == "face");
		CHECK(locker_category(true, false, false, "HEAD", "skin") == "head");
		CHECK(locker_category(false, false, true, "extra", "") == "extra");
		CHECK(locker_category(true, false, true, "extra", "skin") == "extra");
		CHECK(locker_category(false, false, false, "", "skin") == "");
		CHECK(locker_category(false, false, false, "", "ball") == "");
		CHECK(locker_category(false, false, false, "outfit", "skin") == "");
		auto cap = entries_for_pack("1002", true, false, false, "", "skin");
		CHECK(cap.size() == 1 && cap[0].id == "1002" && cap[0].category == "head" && cap[0].part == "hat");
		auto tail = entries_for_pack("1003", false, false, true, "extra", "skin");
		CHECK(tail.size() == 1 && tail[0].id == "1003" && tail[0].category == "extra" && tail[0].part == "back");
		auto face = entries_for_pack("10F0", false, true, false, "", "face");
		CHECK(face.size() == 1 && face[0].id == "10F0" && face[0].category == "face");
		auto bundled = entries_for_pack("1009", true, true, true, "", "skin");
		CHECK(bundled.size() == 3);
		CHECK(bundled[0].id == "1009_hat" && bundled[0].category == "head");
		CHECK(bundled[1].id == "1009_face" && bundled[1].category == "face");
		CHECK(bundled[2].id == "1009_back" && bundled[2].category == "extra");

		std::map<std::string, std::string> slots;
		std::map<std::string, std::string> cats{{"1002","head"},{"1003","extra"},{"10F0","face"},{"1001"," "}};
		auto m1 = migrate(slots, {"1001","1002","1003","10F0"}, {{"1002","head"},{"1003","extra"},{"10F0","face"}});
		CHECK(m1.set_slots["head"] == "1002");
		CHECK(m1.set_slots["extra"] == "1003");
		CHECK(m1.set_slots["face"] == "10F0");
		CHECK(m1.unplaced.empty() && m1.already.empty());

		slots = {{"head","baseballcap"}};
		auto m2 = migrate(slots, {"1002"}, {{"1002","head"}});
		CHECK(m2.set_slots.size() == 1 && m2.set_slots["head2"] == "1002");
		CHECK(m2.set_slots.count("head") == 0);

		slots = {{"head2","1002"}};
		auto m3 = migrate(slots, {"1002"}, {{"1002","head"}});
		CHECK(m3.set_slots.empty() && m3.already.size() == 1 && m3.already[0] == "1002");

		slots = {{"head","a"},{"head2","b"},{"head3","c"}};
		auto m4 = migrate(slots, {"1002"}, {{"1002","head"}});
		CHECK(m4.set_slots.empty() && m4.unplaced.size() == 1 && m4.unplaced[0] == "1002");

		slots = {};
		auto m5 = migrate(slots, {"1002","A100"}, {{"1002","head"},{"A100","head"}});
		CHECK(m5.set_slots["head"] == "1002" && m5.set_slots["head2"] == "A100");

		std::set<std::string> acc_ids{"1002","1003","10F0"};
		auto e1 = sync_equipped({"1001","1002","1003"}, {"1003"}, acc_ids);
		CHECK(e1.size() == 2 && e1[0] == "1001" && e1[1] == "1003");
		auto e2 = sync_equipped({"1001"}, {"1002","10F0"}, acc_ids);
		CHECK(e2.size() == 3 && e2[0] == "1001" && e2[1] == "1002" && e2[2] == "10F0");
		auto e3 = sync_equipped({"1001","1002","1003"}, {"1002","1003"}, acc_ids);
		CHECK(e3.size() == 3 && e3[0] == "1001" && e3[1] == "1002" && e3[2] == "1003");
		auto e4 = sync_equipped({"1001","1002","1002"}, {"1002"}, acc_ids);
		CHECK(e4.size() == 2 && e4[0] == "1001" && e4[1] == "1002");

		auto e5 = sync_equipped({"1001","1002"}, {}, acc_ids);
		CHECK(e5.size() == 1 && e5[0] == "1001");
	}


	{
		namespace GF = jvm::goofy;
		CHECK(GF::params().size() >= 150);
		CHECK(GF::find("ball.gravity") && GF::find("ball.gravity")->def[0] == 11.0 && GF::find("ball.gravity")->target == GF::PHYS);
		CHECK(GF::find("ball.radius") && GF::find("ball.radius")->def[0] == 0.26);
		CHECK(GF::find("player.reach") && GF::find("player.reach")->target == GF::FEEL && GF::find("player.reach")->def[0] == 1.15);
		CHECK(GF::find("flight.air_drag") && GF::find("flight.air_drag")->target == GF::EXT);
		CHECK(GF::find("ball.outline_home") && GF::find("ball.outline_home")->is_color);
		std::set<std::string> ids; bool ranges_ok = true;
		for (auto &p : GF::params()) { ids.insert(p.id); if (!p.is_color && (p.def[0] < p.min || p.def[0] > p.max)) ranges_ok = false; }
		CHECK(ids.size() == GF::params().size());
		CHECK(ranges_ok);
		bool mul = false;
		CHECK(GF::canonical("gravity_scale", &mul) == "ball.gravity" && mul);
		CHECK(GF::canonical("bounce_scale", &mul) == "ball.floor_bounce" && mul);
		CHECK(GF::canonical("big_ball", &mul) == "ball.radius" && mul);
		CHECK(GF::canonical("floor_friction", &mul) == "ball.floor_friction" && !mul);
		CHECK(GF::canonical("player.reach", &mul) == "player.reach" && !mul);
		CHECK(GF::canonical("no_such_thing").empty());
		const GF::Param &g = *GF::find("ball.gravity"), &rad = *GF::find("ball.radius");
		double st[4] = { 11.0, 0, 0, 1 }, out[4]; std::string who;
		GF::Stack s;
		CHECK(!s.resolve(g, st, out) && out[0] == 11.0);
		auto L = [](const char *o, int pr, std::map<std::string, GF::Value> v, GF::Scope sc = GF::Scope::Session) { GF::Layer l; l.owner = o; l.priority = pr; l.values = v; l.scope = sc; return l; };
		auto V = [](double x, bool m = false) { GF::Value v; v.v[0] = x; v.mul = m; return v; };
		s.set(L("user", GF::kUserPriority, { { "ball.gravity", V(0.5, true) }, { "ball.radius", V(2.0, true) } }));
		CHECK(s.resolve(g, st, out, &who) && std::fabs(out[0] - 5.5) < 1e-9 && who == "user");
		s.set_loader_map("1022");
		s.set(L("pack:1022", GF::kPackPriority, { { "ball.gravity", V(0.4, true) } }, GF::Scope::Stage));
		CHECK(s.resolve(g, st, out, &who) && std::fabs(out[0] - 4.4) < 1e-9 && who == "pack:1022");
		double rs[4] = { 0.26, 0, 0, 1 };
		CHECK(s.resolve(rad, rs, out, &who) && std::fabs(out[0] - 0.52) < 1e-9 && who == "user");
		s.set(L("modA", GF::kModPriority, { { "ball.gravity", V(20.0) } }));
		CHECK(s.resolve(g, st, out, &who) && out[0] == 20.0 && who == "modA");
		s.set(L("modB", GF::kModPriority, { { "ball.gravity", V(30.0) } }));
		CHECK(s.resolve(g, st, out, &who) && out[0] == 30.0 && who == "modB");
		s.set(L("modA", GF::kModPriority, { { "ball.gravity", V(21.0) } }));
		CHECK(s.resolve(g, st, out, &who) && out[0] == 21.0 && who == "modA");
		s.set(L("modC", 1000, { { "ball.gravity", V(999.0) } }));
		CHECK(s.resolve(g, st, out) && out[0] == 55.0);
		s.clear("modC"); s.clear("modA"); s.clear("modB");
		s.set_fixed(true);
		CHECK(s.force_defaults() && !s.resolve(g, st, out, &who) && out[0] == 11.0 && who == "fixed");
		CHECK(!s.resolve(rad, rs, out) && out[0] == 0.26);
		auto dropped = s.set_loader_map("");
		CHECK(dropped.size() == 1 && dropped[0] == "pack:1022");
		CHECK(!s.force_defaults() && s.resolve(g, st, out, &who) && std::fabs(out[0] - 5.5) < 1e-9 && who == "user");
		s.set_fixed(false); s.set_loader_map("1028");
		CHECK(s.resolve(g, st, out, &who) && who == "user");
		s.set(L("user", GF::kUserPriority, {}));
		CHECK(!s.resolve(g, st, out) && s.layers().empty());
		s.set(L("x", 300, { { "ball.gravity", V(11.0) } }));
		CHECK(!s.resolve(g, st, out));
		const GF::Param &col = *GF::find("ball.outline_home");
		double cs[4] = { 0.35, 1.0, 0.55, 1.0 }; GF::Value cv; cv.v[0] = 2; cv.v[1] = 0; cv.v[2] = 0; cv.v[3] = 1;
		s.set(L("x", 300, { { "ball.outline_home", cv } }));
		CHECK(s.resolve(col, cs, out) && out[0] == 1.0 && out[1] == 0.0);
	}


	{
		namespace GF = jvm::goofy;
		CHECK(GF::ball_param_count() == 156 && GF::params().size() == 156 + 195);
		const GF::Param *ms = GF::find("player.move_speed"), *cq = GF::find("player.contact_queue_steps");
		const GF::Param *bs = GF::find("player.body_scale"), *ic = GF::find("player.indicator_color");
		const GF::Param *reach = GF::find("player.reach"), *gr = GF::find("ball.gravity");
		CHECK(ms && ms->target == GF::FEEL && ms->def[0] == 5.9 && GF::is_player_library(*ms) && GF::is_player_param(*ms));
		CHECK(cq && std::string(cq->apply) == "live_int" && cq->def[0] == 6.0);
		CHECK(bs && bs->target == GF::EXT && bs->min == 0.25 && bs->max == 4.0);
		CHECK(ic && ic->is_color && GF::is_player_library(*ic));
		CHECK(reach && !GF::is_player_library(*reach) && GF::is_player_param(*reach));
		CHECK(gr && !GF::is_player_library(*gr) && !GF::is_player_param(*gr));
		bool mul = true;
		CHECK(GF::canonical("player_scale", &mul) == "player.body_scale" && !mul);
		std::set<std::string> secs;
		for (auto &p : GF::params()) if (GF::is_player_library(p)) secs.insert(p.section);
		for (const char *want : { "movement", "jump", "sprint_stamina", "dive_fall", "block", "serve", "animation", "visuals", "contact_timing", "body" })
			CHECK(secs.count(want) == 1);

		auto L = [](const char *o, int pr, std::map<std::string, GF::Value> v) { GF::Layer l; l.owner = o; l.priority = pr; l.values = v; return l; };
		auto V = [](double x, bool m = false) { GF::Value v; v.v[0] = x; v.mul = m; return v; };
		GF::Stack s;
		double gs[4] = { 11.0, 0, 0, 1 }, mst[4] = { 5.9, 0, 0, 1 }, out[4]; std::string who;
		s.set(L("user", GF::kUserPriority, { { "ball.gravity", V(20.0) }, { "player.move_speed", V(9.0) } }));
		s.set_loader_map("1022"); s.set_fixed(true);
		CHECK(!s.resolve(*gr, gs, out) && out[0] == 11.0);
		CHECK(s.resolve(*ms, mst, out, &who) && out[0] == 9.0 && who == "user");
		s.set_fixed(false); s.set_loader_map("");

		GF::Value v;
		double in1[1] = { 1e9 };
		CHECK(GF::sanitize_host_value(*gr, in1, 1, v) && v.v[0] == gr->max && !v.mul);
		double nan1[1] = { std::nan("") };
		CHECK(!GF::sanitize_host_value(*gr, nan1, 1, v));
		double inf1[1] = { INFINITY };
		CHECK(!GF::sanitize_host_value(*gr, inf1, 1, v));
		double neg[1] = { -5.0 };
		CHECK(GF::sanitize_host_value(*ms, neg, 1, v) && v.v[0] == ms->min);
		double frac[1] = { 7.6 };
		CHECK(GF::sanitize_host_value(*cq, frac, 1, v) && v.v[0] == 8.0);
		double two[2] = { 1, 2 };
		CHECK(!GF::sanitize_host_value(*gr, two, 2, v));

		double c3[3] = { 0.5, 0.5, 0.5 };
		CHECK(GF::is_visual(*ic) && !GF::sanitize_host_value(*ic, c3, 3, v));
		const GF::Param *trail = GF::find("ball.trail_min_speed");
		CHECK(trail && GF::is_visual(*trail) && !GF::sanitize_host_value(*trail, in1, 1, v));
		CHECK(!GF::is_visual(*gr) && !GF::is_visual(*ms) && !GF::is_visual(*cq));
		{
			int vis = 0, host = 0;
			for (auto &p : GF::params()) (GF::is_visual(p) ? vis : host)++;
			CHECK(vis + host == int(GF::params().size()) && vis > 0 && host > vis);
			for (const char *id : { "ball.radius", "ball.gravity", "flight.air_drag", "flight.vertical_drag_scale", "ball.floor_bounce",
					"ball.roll_drag", "flight.max_speed", "player.jump_min_velocity", "player.body_scale" }) {
				const GF::Param *p = GF::find(id);
				CHECK(p && !GF::is_visual(*p));
			}
		}

		double ts = 0;
		CHECK(GF::sanitize_time_scale(0.5, ts) && ts == 0.5);
		CHECK(GF::sanitize_time_scale(100.0, ts) && ts == GF::kTimeScaleMax);
		CHECK(GF::sanitize_time_scale(-3.0, ts) && ts == GF::kTimeScaleMin);
		CHECK(!GF::sanitize_time_scale(std::nan(""), ts) && !GF::sanitize_time_scale(INFINITY, ts));


		GF::Value cv; cv.v[0] = 1.0; cv.v[1] = 0.0; cv.v[2] = 0.0; cv.v[3] = 1.0;
		s.set(L("user", GF::kUserPriority, { { "ball.gravity", V(20.0) }, { "player.move_speed", V(9.0) }, { "player.indicator_color", cv } }));
		double ics[4] = { 0.2, 0.9, 1.0, 1.0 };
		s.set_loader_map("1022"); s.set_fixed(true);
		s.set_host_mode(true);
		CHECK(!s.force_defaults());
		CHECK(!s.resolve(*gr, gs, out, &who) && out[0] == 11.0 && who == "host");
		CHECK(!s.resolve(*ms, mst, out) && out[0] == 5.9);
		CHECK(s.resolve(*ic, ics, out, &who) && out[0] == 1.0 && out[1] == 0.0 && who == "user");
		std::map<std::string, GF::Value> hv;
		GF::Value h; double hin[1] = { 30.0 };
		CHECK(GF::sanitize_host_value(*gr, hin, 1, h)); hv["ball.gravity"] = h;
		s.set_host_values(hv);
		CHECK(s.resolve(*gr, gs, out, &who) && out[0] == 30.0 && who == "host");
		CHECK(!s.resolve(*ms, mst, out) && out[0] == 5.9);
		s.set_host_mode(false);
		CHECK(s.host_values().size() == 1 && s.force_defaults());
		s.set_fixed(false);
		CHECK(s.resolve(*gr, gs, out, &who) && out[0] == 20.0 && who == "user");
		s.set_host_mode(true);
		CHECK(s.resolve(*gr, gs, out) && out[0] == 30.0);
		s.set_host_mode(false); s.clear_host_values();
		CHECK(s.host_values().empty());
		CHECK(GF::kHostPayloadVersion == 2 && GF::kHostPayloadMaxBytes == 32768);
	}

	{
		namespace CS = jvm::content;
		auto S = [](const std::string &t) { return CS::to_hex(CS::sha256(reinterpret_cast<const uint8_t *>(t.data()), t.size())); };
		CHECK(S("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
		CHECK(S("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
		CHECK(S("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
		{ std::string m(1000000, 'a'); CHECK(S(m) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"); }
		{ CS::Sha256 st; std::string a = "ab", b = "c"; st.update((const uint8_t *)a.data(), 2); st.update((const uint8_t *)b.data(), 1); CHECK(CS::to_hex(st.finish()) == S("abc")); }
		CHECK(CS::from_hex(S("abc")) && CS::to_hex(*CS::from_hex(S("abc"))) == S("abc"));
		CHECK(!CS::from_hex("abc") && !CS::from_hex(std::string(64, 'g')));

		CS::Hash h1 = *CS::from_hex(S("one")), h2 = *CS::from_hex(S("two"));
		std::vector<CS::FileEntry> fa = { { "skin.json", h1, 10 }, { "ball_tex.png", h2, 20 } }, fb = { fa[1], fa[0] };
		CHECK(CS::tree_hash(fa) == CS::tree_hash(fb) && CS::tree_size(fa) == 30);
		auto fc = fa; fc[1].sha = h1; CHECK(CS::tree_hash(fc) != CS::tree_hash(fa));
		auto fd = fa; fd[0].size = 11; CHECK(CS::tree_hash(fd) != CS::tree_hash(fa));
		auto fe = fa; fe[0].path = "Skin.json"; CHECK(CS::tree_hash(fe) != CS::tree_hash(fa));

		CS::Offer o; o.items.push_back({ 0x1011, CS::Kind::Ball, h1, 12345 }); o.items.push_back({ 0x1002, CS::Kind::Accessory, h2, 7 });
		auto ob = CS::encode(o);
		CHECK(ob.size() == 7 + 2 * 39 && ob[5] == CS::kTypeOffer);
		auto od = CS::decode(ob);
		CHECK(od && !od->relayed && od->items.size() == 2 && od->items[0] == o.items[0] && od->items[1] == o.items[1]);
		o.relayed = true; o.peer = 1234567;
		auto rd = CS::decode(CS::encode(o));
		CHECK(rd && rd->relayed && rd->peer == 1234567 && rd->items.size() == 2);
		CS::Offer empty; auto ed = CS::decode(CS::encode(empty)); CHECK(ed && ed->items.empty());
		{ auto t = ob; t.pop_back(); CHECK(!CS::decode(t)); }
		{ auto t = ob; t.push_back(0); CHECK(!CS::decode(t)); }
		{ auto t = ob; t[6] = 17; CHECK(!CS::decode(t)); }
		{ auto t = ob; t[9] = 9; CHECK(!CS::decode(t)); }
		{ auto t = ob; t[4] = 2; CHECK(!CS::decode(t)); }
		{ auto t = ob; t[5] = 3; CHECK(!CS::decode(t)); }
		{ auto e = jvm::skins::encode(jvm::skins::Packet{}); CHECK(!CS::decode(e)); }
		{ CS::Offer big; for (int i = 0; i < 40; i++) big.items.push_back({ uint16_t(i), CS::Kind::Outfit, h1, 1 }); CHECK(CS::decode(CS::encode(big))->items.size() == CS::kMaxItems); }
		CHECK(!jvm::skins::decode(ob));

		const std::string B = CS::kRawBase;
		CHECK(std::string(CS::kManifestUrl) == B + "manifest.json" && CS::url_allowed(CS::kManifestUrl));
		CHECK(B == "https://raw.githubusercontent.com/Ashtonite365/just-volleyball-assets/main/");
		CHECK(CS::url_allowed(B + "balls/1011_beach/pack.zip"));
		for (const char *u : { "https://raw.githubusercontent.com/Ashtonite365/just-volleyball-assets/mainx/a.zip", "https://evil.example/balls/a/pack.zip",
				"http://raw.githubusercontent.com/Ashtonite365/just-volleyball-assets/main/a/pack.zip", "https://raw.githubusercontent.com/Ashtonite365/other/main/a/pack.zip" })
			CHECK(!CS::url_allowed(u));
		for (const char *r : { "../x/pack.zip", "a/../../b/pack.zip", "a//pack.zip", "a/pack.zip?x=1", "a/pack.zip#f", "a/%2e%2e/pack.zip", "a\\b/pack.zip", "a b/pack.zip", "@evil/pack.zip", "" })
			CHECK(!CS::url_allowed(B + r));
		CHECK(CS::safe_repo_path("balls/1011_beach") && CS::safe_repo_path("models/x/bean-2"));
		for (const char *p : { "1011_beach", "/balls/x", "balls/x/", "balls/../x", "balls/.hidden", "a/b/c/d", "balls/x y", "balls\\x" }) CHECK(!CS::safe_repo_path(p));
		CHECK(CS::cache_folder("balls/1011_beach") == "1011_beach");

		auto item = [&](const char *id, const char *type, const char *path) {
			CS::RawManifestItem r; r.id = id; r.name = "N"; r.type = type; r.path = path; r.size = 1000;
			r.sha256 = S(std::string("zip") + id); r.download_url = B + path + "/pack.zip";
			r.files = { { "skin.json", { S("j"), 100.0 } }, { "ball_tex.png", { S("p"), 800.0 } } };
			return r;
		};
		std::vector<CS::RawManifestItem> raw = { item("1011", "ball", "balls/1011_beach"), item("1001", "outfit", "outfits/1001_crimson") };
		auto cat = CS::validate_manifest(1, raw);
		CHECK(cat.items.size() == 2 && cat.errors.empty());
		CHECK(cat.items[0x1011].kind == CS::Kind::Ball && cat.items[0x1011].folder == "1011_beach" && cat.items[0x1011].tree_bytes == 900);
		CHECK(cat.items[0x1011].url == B + "balls/1011_beach/pack.zip" && cat.items[0x1001].kind == CS::Kind::Outfit);
		CHECK(CS::validate_manifest(2, raw).items.empty() && CS::validate_manifest(2, raw).errors.size() == 1);
		auto one_bad = [&](auto mut) { auto r = raw; mut(r[0]); auto c = CS::validate_manifest(1, r); return c.items.size() == 1 && c.errors.size() == 1; };
		CHECK(one_bad([](auto &r) { r.id = "10111"; }));
		CHECK(one_bad([](auto &r) { r.id = "zz11"; }));
		CHECK(one_bad([](auto &r) { r.type = "script"; }));
		CHECK(one_bad([](auto &r) { r.sha256 = "abc"; }));
		CHECK(one_bad([](auto &r) { r.size = -1; }));
		CHECK(one_bad([](auto &r) { r.size = 0; }));
		CHECK(one_bad([](auto &r) { r.size = double(CS::kMaxPackBytes) + 1; }));
		CHECK(one_bad([](auto &r) { r.size = 10.5; }));
		CHECK(one_bad([&](auto &r) { r.download_url = "https://evil.example/pack.zip"; }));
		CHECK(one_bad([&](auto &r) { r.download_url = B + "balls/1012_other/pack.zip"; }));
		CHECK(one_bad([&](auto &r) { r.path = "../balls"; r.download_url = B + "../balls/pack.zip"; }));
		CHECK(one_bad([](auto &r) { r.files.push_back({ "payload.dll", { std::string(64, 'a'), 1.0 } }); }));
		CHECK(one_bad([](auto &r) { r.files.push_back({ "run.gd", { std::string(64, 'a'), 1.0 } }); }));
		CHECK(one_bad([](auto &r) { r.files.push_back({ "../x.png", { std::string(64, 'a'), 1.0 } }); }));
		CHECK(one_bad([](auto &r) { r.files[0].second.first = "nothex"; }));
		CHECK(one_bad([](auto &r) { r.files[0].second.second = double(CS::kMaxFileBytes) + 1; }));
		CHECK(one_bad([](auto &r) { r.files.clear(); }));
		{ auto r = raw; r.push_back(item("1011", "ball", "balls/1011_copy")); auto c = CS::validate_manifest(1, r); CHECK(c.items.size() == 2 && c.items[0x1011].folder == "1011_beach" && c.errors.size() == 1); }
		CHECK(CS::kind_from_type("hat") == CS::Kind::Accessory && CS::kind_from_type("stage") == CS::Kind::Court && CS::kind_from_type("model") == CS::Kind::Model && !CS::kind_from_type("exe"));

		for (const char *ok : { "skin.json", "ball_tex.png", "Ball_Tex.PNG", "tex/inner.png", "a/b/c.glb", "cap.glb" }) CHECK(CS::safe_entry(ok) && *CS::safe_entry(ok) == ok);
		CHECK(CS::safe_entry("tex/") && CS::safe_entry("tex/")->empty());
		for (const char *bad : { "../evil.png", "a/../../evil.png", "/etc/x.png", "\\evil.png", "a\\..\\b.png", "C:/x.png", "C:x.png", "./x.png", "a//b.png",
				"a/./b.png", "a/b/c/d.png", "x.dll", "x.gd", "x.exe", "x.pck", "x.so", "x.png.exe", "x", ".png", "x..png", "../", "/", "", "x\n.png", "x\x01.png",
				"\\\\server\\share\\x.png" })
			CHECK(!CS::safe_entry(bad));
		CHECK(!CS::safe_entry(std::string(97, 'a')));
		CHECK(CS::allowed_ext("A.JSON") && CS::allowed_ext("m.glb") && !CS::allowed_ext("m.gltf") && !CS::allowed_ext("m.tscn") && !CS::allowed_ext("m.res"));

		const auto &ci = cat.items[0x1011];
		CS::Item want{ 0x1011, CS::Kind::Ball, ci.tree, uint32_t(ci.tree_bytes) };
		std::map<uint16_t, CS::Hash> inst;
		CHECK(CS::decide(want, inst, cat) == CS::Decision::Download);
		{ auto w = want; w.hash = h1; CHECK(CS::decide(w, inst, cat) == CS::Decision::HashMismatch); }
		{ auto w = want; w.size += 1; CHECK(CS::decide(w, inst, cat) == CS::Decision::HashMismatch); }
		{ auto w = want; w.kind = CS::Kind::Outfit; CHECK(CS::decide(w, inst, cat) == CS::Decision::HashMismatch); }
		{ auto w = want; w.id = 0x1999; CHECK(CS::decide(w, inst, cat) == CS::Decision::NotInCatalog); }
		inst[0x1011] = ci.tree; CHECK(CS::decide(want, inst, cat) == CS::Decision::Have);
		inst[0x1011] = h1; CHECK(CS::decide(want, inst, cat) == CS::Decision::LocalDiffers);
		CHECK(std::string(CS::decision_name(CS::Decision::HashMismatch)).find("hash mismatch") == 0);
	}
	std::printf("%d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
