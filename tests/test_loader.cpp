

#include "loader/bridge_logic.hpp"
#include <cstdio>
#include <cstring>

using namespace jvm::loader;
static int fails = 0, passes = 0;
static void check(bool c, const char *what) { if (c) { passes++; std::printf("PASS %s\n", what); } else { fails++; std::printf("FAIL %s\n", what); } }

static BridgeState on_map(const char *id = "1022") {
	BridgeState s; s.goofy_usable = true; s.map = id; s.has_block = true; s.block_sig = "100"; return s;
}
static BridgeState after(const BridgeState &s0) {
	BridgeState s = s0; Decision d = decide(s0);
	if (!d.clear_pack.empty()) { s.applied_pack.clear(); s.applied_sig.clear(); }
	if (d.set_map) { s.told = true; s.told_map = s.map; }
	if (d.apply) { s.applied_pack = s.map; s.applied_sig = s.block_sig; }
	return s;
}

int main() {
	check(api_compatible("1.0") && api_compatible("1.7") && api_compatible("1"), "API 1.x accepted");
	check(!api_compatible("2.0") && !api_compatible("") && !api_compatible("0.9") && !api_compatible("x.1"), "other majors / junk refused");
	{ auto d = decide(on_map()); check(d.set_map && d.apply && d.clear_pack.empty(), "enter map with goofy section -> set_loader_map + apply"); }
	{ auto s = after(on_map()); auto d = decide(s); check(!d.set_map && !d.apply && d.clear_pack.empty(), "steady state -> no calls every tick"); }
	{ auto s = on_map(); s.goofy_usable = false; auto d = decide(s); check(!d.set_map && !d.apply && std::strstr(d.why, "ignored"), "goofy absent / no API -> nothing called"); }
	{ auto s = on_map(); s.has_block = false; auto d = decide(s); check(d.set_map && !d.apply, "map without goofy section -> set_loader_map only (drives Ball FIXED)"); }
	{ auto s = after(on_map()); s.map = ""; s.has_block = false; auto d = decide(s); check(d.clear_pack == "1022" && d.set_map && !d.apply, "leave map -> clear_pack_params + set_loader_map('')"); }
	{ auto s = after(on_map()); s.map = "1028"; auto d = decide(s); check(d.clear_pack == "1022" && d.set_map && d.apply, "switch map -> clear old, set map, apply new"); }
	{ auto s = after(on_map()); s.block_sig = "200"; auto d = decide(s); check(d.apply && d.clear_pack.empty() && !d.set_map, "edited skin.json -> re-apply in place"); }
	{ BridgeState s; s.goofy_usable = true; auto d = decide(s); check(d.set_map && !d.apply, "stock map at start -> set_loader_map('') once"); auto s2 = after(s); check(!decide(s2).set_map, "...and not again"); }
	std::printf("RESULT %d passed, %d failed\n", passes, fails);
	return fails ? 1 : 0;
}
