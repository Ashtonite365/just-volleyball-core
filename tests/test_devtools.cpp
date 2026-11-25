

#include "../src/gate/const_check.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace jvm::consts;
static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static bool has(const std::vector<Finding> &f, const char *code, const char *name = nullptr) {
	return std::any_of(f.begin(), f.end(), [&](const Finding &x) { return x.code == code && (!name || x.name == name); });
}
static Live bypassed(double v = 1.0) {
	Live l; l.available = true;
	for (auto &n : gameplay_names()) { l.statics[n] = n == "JUMP_SPIKE_GRACE_US" ? 1000 : v; Spec s; s.has_stock = true; s.stock = n == "JUMP_SPIKE_GRACE_US" ? 1000 : 1.0; s.is_int = n == "JUMP_SPIKE_GRACE_US"; l.specs[n] = s; }
	for (auto &n : protocol_names()) l.still_const.insert(n);
	return l;
}
static Write W(const char *n, double v, const char *src = "goofy") { Write w; w.name = n; w.value = v; w.source = src; w.is_integral = std::fabs(v - std::round(v)) < 1e-9; w.finite = std::isfinite(v); return w; }

int main() {
	CHECK(gameplay_names().size() == 34);
	CHECK(classify("SET_DUMP_RISE_TAP") == Kind::Gameplay);
	CHECK(classify("NET_FLOATS") == Kind::Protocol && classify("HIGH_FIVE_ON") == Kind::Protocol && classify("CALLOUT_SET") == Kind::Protocol && classify("QUICK_CHAT") == Kind::Protocol);
	CHECK(classify("CALLOUT_BURST") == Kind::Visual && classify("CALLOUT_COOLDOWN_MS") == Kind::Visual && classify("DIVE_DENY_TIME") == Kind::Visual && classify("POSE_SPIKE") == Kind::Visual);
	CHECK(classify("WHATEVER") == Kind::Unknown);
	CHECK(folded_readers("NET_FLOATS").size() == 5 && folded_readers("BLOCK_TAPE_SLACK").size() == 1 && folded_readers("BLOCK_TURN").empty());
	double lo, hi; Spec s; s.has_stock = true; s.stock = 2.8;
	CHECK(default_bounds(s, lo, hi) && lo == 0.0 && std::fabs(hi - 11.2) < 1e-9);
	s.stock = -0.5; CHECK(default_bounds(s, lo, hi) && lo == -2.0 && hi == 0.0);
	s.stock = 0; CHECK(default_bounds(s, lo, hi) && lo == -1 && hi == 1);
	Spec none; CHECK(!default_bounds(none, lo, hi));
	Context offline; offline.mode = "Free Play";

	Live l = bypassed();
	CHECK(check_write(W("SET_DUMP_RISE_TAP", 3.5), offline, l).empty());
	CHECK(has(check_write(W("SET_DUMP_RISE_TAP", 9.0), offline, l), "OUT_OF_RANGE"));
	CHECK(has(check_write(W("SET_DUMP_RISE_TAP", -1.0), offline, l), "OUT_OF_RANGE"));
	CHECK(has(check_write(W("JUMP_SPIKE_GRACE_US", 1500.5), offline, l), "BAD_TYPE"));
	CHECK(check_write(W("JUMP_SPIKE_GRACE_US", 1500), offline, l).empty());
	Write nan = W("SPIKE_RECOVER", std::numeric_limits<double>::quiet_NaN());
	CHECK(has(check_write(nan, offline, l), "BAD_VALUE"));
	Write str = W("SPIKE_RECOVER", 0); str.is_number = false;
	CHECK(has(check_write(str, offline, l), "BAD_TYPE"));
	CHECK(has(check_write(W("NET_FLOATS", 9), offline, l), "PROTOCOL_NAME"));
	CHECK(has(check_write(W("HIGH_FIVE_ON", 0), offline, l), "PROTOCOL_NAME"));
	CHECK(has(check_write(W("POSE_SPIKE", 1), offline, l), "NOT_GAMEPLAY"));
	CHECK(has(check_write(W("NOPE", 1), offline, l), "UNKNOWN_NAME"));
	Live partial = l; partial.statics.erase("BLOCK_TURN"); partial.still_const.insert("BLOCK_TURN");
	CHECK(has(check_write(W("BLOCK_TURN", 1.5), offline, partial), "CONST_NOT_STATIC"));
	Live off; off.available = false; off.reason = "version pin mismatch";
	auto f = check_write(W("BLOCK_TURN", 1.5), offline, off);
	CHECK(has(f, "CAP_UNAVAILABLE") && f[0].detail.find("version pin") != std::string::npos);
	Context pub; pub.mode = "public"; pub.cap_open = false;
	CHECK(has(check_write(W("BLOCK_TURN", 1.5), pub, l), "CAP_UNAVAILABLE"));
	Context client; client.mode = "LAN"; client.synced_lobby = true; client.is_host = false;
	CHECK(has(check_write(W("BLOCK_TURN", 1.5), client, l), "NON_HOST_WRITE"));
	Context host = client; host.is_host = true;
	CHECK(check_write(W("BLOCK_TURN", 1.5), host, l).empty());
	Context inmatch = host; inmatch.match_active = true;
	CHECK(has(check_write(W("BLOCK_TURN", 1.5), inmatch, l), "MID_MATCH_WRITE"));
	Context offmatch = offline; offmatch.match_active = true;
	CHECK(has(check_write(W("BLOCK_TURN", 1.0), offmatch, l), "MID_MATCH_WRITE"));
	CHECK(has_error(check_write(W("BLOCK_TURN", 1.0), offmatch, l)));

	Expect e;
	CHECK(audit(l, offline, e).empty());
	Live chg = l; chg.statics["BLOCK_TURN"] = 1.5;
	CHECK(has(audit(chg, offline, e), "UNTRACKED_WRITE", "BLOCK_TURN"));
	e.approved["BLOCK_TURN"] = 1.5;
	CHECK(audit(chg, offline, e).empty());
	CHECK(has(audit(chg, pub, e), "NOT_STOCK_WHILE_CLOSED", "BLOCK_TURN"));

	Expect m = e; m.start_valid = true; m.start_set = chg.statics;
	CHECK(audit(chg, inmatch, m).empty());
	Live later = chg; later.statics["SPIKE_RECOVER"] = 1.2;
	auto fa = audit(later, inmatch, m);
	CHECK(has(fa, "MID_MATCH_CHANGE", "SPIKE_RECOVER") && has(fa, "UNTRACKED_WRITE", "SPIKE_RECOVER"));

	Context cm = client; cm.match_active = true;
	Expect hc = m; hc.host_valid = true; hc.host_set = chg.statics;
	CHECK(audit(chg, cm, hc).empty());
	hc.host_set["BLOCK_TURN"] = 1.25;
	auto fh = audit(chg, cm, hc);
	CHECK(has(fh, "HOST_MISMATCH", "BLOCK_TURN") && !has(fh, "HOST_MISMATCH", "SPIKE_RECOVER"));
	hc.host_set["NET_FLOATS"] = 9;
	CHECK(has(audit(chg, cm, hc), "HOST_MISMATCH", "NET_FLOATS"));

	Live proto = l; proto.statics["NET_FLOATS"] = 8;
	auto fp = audit(proto, offline, Expect());
	CHECK(has(fp, "PROTOCOL_REWRITTEN", "NET_FLOATS") && fp[0].detail.find("net_sync.gd") != std::string::npos);
	Live tape = l; tape.statics["BLOCK_TAPE_SLACK"] = 1.5;
	Expect te; te.approved["BLOCK_TAPE_SLACK"] = 1.5;
	auto ft = audit(tape, offline, te);
	CHECK(has(ft, "FOLDED_COPY_STALE", "BLOCK_TAPE_SLACK") && !has_error(ft));

	Live stock; stock.available = false; for (auto &n : gameplay_names()) stock.still_const.insert(n);
	CHECK(audit(stock, pub, Expect()).empty());

	CHECK(has(audit(partial, offline, e), "CONST_NOT_REWRITTEN", "BLOCK_TURN"));

	Live big = l; big.statics["SPIKE_RECOVER"] = 7.0;
	Expect be; be.approved["SPIKE_RECOVER"] = 7.0;
	CHECK(has(audit(big, offline, be), "OUT_OF_RANGE", "SPIKE_RECOVER"));
	std::printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
