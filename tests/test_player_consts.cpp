

#include "../src/gate/player_consts.hpp"
#include <cstdio>
using namespace jvm::pconsts;
static int g_pass = 0, g_fail = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)


static ActiveFn roster(std::map<std::string, std::string> why_missing) {
	return [why_missing](const std::string &p, std::string *why) {
		auto it = why_missing.find(p);
		if (it == why_missing.end()) return true;
		if (why) *why = it->second;
		return false;
	};
}
static double V(const Lock &l, const char *n) { return l.values().at(n); }

int main() {
	CHECK(specs().size() == 34);
	CHECK(find("SET_DUMP_RISE_TAP") && find("SET_DUMP_RISE_TAP")->stock == 2.8);
	CHECK(!find("NET_FLOATS") && !find("DIVE_DENY_TIME"));
	CHECK(manifest_id("26924091") == "player.consts@26924091");


	Lock host;
	CHECK(!host.locked());
	CHECK(host.set("SET_DUMP_RISE_TAP", 5.0, "api").ok);
	CHECK(host.set("JUMP_SPIKE_GRACE_US", 200000.4, "ui").ok && host.saved().at("JUMP_SPIKE_GRACE_US") == 200000.0);
	CHECK(host.set("SIDE_DIVE_DEG", 500.0, "ui").ok && host.saved().at("SIDE_DIVE_DEG") == 89.0);
	CHECK(!host.set("NOPE", 1.0, "api").ok);
	CHECK(!host.set("BLOCK_TURN", 0.0 / 0.0, "api").ok);
	CHECK(host.reset("SIDE_DIVE_DEG", "ui").ok && !host.saved().count("SIDE_DIVE_DEG"));
	CHECK(V(host, "SET_DUMP_RISE_TAP") == 2.8);


	Plan pl = host.begin_match_host(7, Context::Host, {"Ann", "Cara"}, roster({}));
	CHECK(host.locked() && pl.apply && pl.missing.empty() && pl.notice.empty());
	CHECK(V(host, "SET_DUMP_RISE_TAP") == 5.0 && V(host, "JUMP_SPIKE_GRACE_US") == 200000.0);


	Result w = host.set("SET_DUMP_RISE_TAP", 9.0, "api");
	CHECK(!w.ok && w.error.find("locked") != std::string::npos);
	CHECK(!host.set("BLOCK_TURN", 3.0, "goofy-ui").ok);
	CHECK(!host.reset("SET_DUMP_RISE_TAP", "api").ok);
	CHECK(host.rejected() == 3 && host.warnings().size() == 3);
	CHECK(V(host, "SET_DUMP_RISE_TAP") == 5.0 && host.saved().at("SET_DUMP_RISE_TAP") == 5.0);


	std::string bytes = encode_plan(pl);
	Lock ann;
	ann.set("SET_DUMP_RISE_TAP", 1.0, "ui");
	ann.begin_match_client(1);
	CHECK(V(ann, "SET_DUMP_RISE_TAP") == 2.8);
	CHECK(ann.on_plan(bytes, true).ok);
	CHECK(V(ann, "SET_DUMP_RISE_TAP") == 5.0 && V(ann, "JUMP_SPIKE_GRACE_US") == 200000.0);
	Plan evil = pl; evil.values["SET_DUMP_RISE_TAP"] = 11.0;
	Result r2 = ann.on_plan(encode_plan(evil), true);
	CHECK(!r2.ok && V(ann, "SET_DUMP_RISE_TAP") == 5.0 && ann.rejected() == 1);
	CHECK(!ann.set("SET_DUMP_RISE_TAP", 1.0, "api").ok);
	Lock c2; c2.begin_match_client(1);
	CHECK(!c2.on_plan(bytes, false).ok && V(c2, "SET_DUMP_RISE_TAP") == 2.8);
	Lock c3; CHECK(c3.on_plan(bytes, true).ok); c3.begin_match_client(1);
	CHECK(V(c3, "SET_DUMP_RISE_TAP") == 5.0);


	Plan nxt = pl; nxt.match_id = 2; nxt.values["SET_DUMP_RISE_TAP"] = 6.5;
	CHECK(c3.on_plan(encode_plan(nxt), true).ok && V(c3, "SET_DUMP_RISE_TAP") == 5.0);
	c3.end_match(); c3.begin_match_client(2);
	CHECK(V(c3, "SET_DUMP_RISE_TAP") == 6.5 && c3.plan_received());
	Lock c4; CHECK(c4.on_plan(bytes, true).ok); c4.clear_pending(); c4.begin_match_client(1);
	CHECK(V(c4, "SET_DUMP_RISE_TAP") == 2.8 && !c4.plan_received());
	Lock c5; CHECK(c5.on_plan(bytes, true, 100.0).ok); c5.begin_match_client(1, 100.0 + kEarlyPlanMaxAgeS + 1.0);
	CHECK(V(c5, "SET_DUMP_RISE_TAP") == 2.8 && !c5.plan_received());
	Lock c6; CHECK(c6.on_plan(bytes, true, 100.0).ok); c6.begin_match_client(1, 101.5);
	CHECK(V(c6, "SET_DUMP_RISE_TAP") == 5.0 && c6.plan_received());


	std::map<std::string, double> back = host.end_match();
	CHECK(!host.locked() && back.size() == 34);
	bool exact = true;
	for (const Spec &s : specs()) exact = exact && back.at(s.name) == s.stock && V(host, s.name) == s.stock;
	CHECK(exact);
	CHECK(host.saved().at("SET_DUMP_RISE_TAP") == 5.0);
	CHECK(host.set("SET_DUMP_RISE_TAP", 6.0, "api").ok);
	ann.end_match(); CHECK(V(ann, "SET_DUMP_RISE_TAP") == 2.8);


	Plan off = host.begin_match_host(8, Context::Host, {"Ann", "Bob", "Cara"},
			roster({{"Bob", "missing"}, {"Cara", "game build 26900000 (host 26924091)"}}));
	CHECK(!off.apply && off.values.empty() && off.missing.size() == 2);
	CHECK(off.notice.find("OFF this match") != std::string::npos && off.notice.find("Bob (missing)") != std::string::npos &&
			off.notice.find("Cara (game build 26900000 (host 26924091))") != std::string::npos);
	CHECK(V(host, "SET_DUMP_RISE_TAP") == 2.8);
	Lock ann2; ann2.begin_match_client(2); CHECK(ann2.on_plan(encode_plan(off), true).ok);
	CHECK(V(ann2, "SET_DUMP_RISE_TAP") == 2.8);
	CHECK(!host.set("SET_DUMP_RISE_TAP", 7.0, "api").ok);
	host.end_match();


	Plan tmp;
	CHECK(decode_plan(encode_plan(off), tmp) && !tmp.apply && tmp.values.empty());
	CHECK(!decode_plan("JVPC 2\nmatch=1\napply=1\n", tmp));
	CHECK(!decode_plan("JVPC 1\nmatch=1\napply=1\nNET_FLOATS=3\n", tmp));
	CHECK(!decode_plan("JVPC 1\nmatch=1\napply=1\nBLOCK_TURN=nan\n", tmp));
	CHECK(!decode_plan("JVPC 1\nmatch=1\napply=1\nBLOCK_TURN=1\nBLOCK_TURN=2\n", tmp));
	CHECK(!decode_plan("JVPC 1\nmatch=1\napply=0\nBLOCK_TURN=1\n", tmp));
	CHECK(decode_plan("JVPC 1\nmatch=1\napply=1\nBLOCK_TURN=99\n", tmp) && tmp.values["BLOCK_TURN"] == 8.0);
	CHECK(!decode_plan(std::string(20000, 'x'), tmp));


	Plan pub = host.begin_match_host(9, Context::Public, {"Ann"}, roster({{"Ann", "missing"}}));
	CHECK(!pub.apply && pub.notice.empty() && V(host, "SET_DUMP_RISE_TAP") == 2.8);
	host.end_match();
	Plan solo = host.begin_match_host(10, Context::Offline, {}, roster({}));
	CHECK(solo.apply && V(host, "SET_DUMP_RISE_TAP") == 6.0 && host.locked());
	host.end_match();
	Lock plain; Plan pp = plain.begin_match_host(1, Context::Host, {"Bob"}, roster({{"Bob", "missing"}}));
	CHECK(pp.notice.empty() && pp.missing.size() == 1);

	std::printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
