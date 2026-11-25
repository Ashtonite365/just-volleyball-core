#include "version_pin.hpp"
#include <cctype>

namespace jvm::pin {

std::string lower_hex(std::string s) {
	for (auto &c : s) c = (char)std::tolower((unsigned char)c);
	return s;
}

std::string parse_acf_buildid(const std::string &t) {
	const std::string key = "\"buildid\"";
	size_t p = t.find(key);
	if (p == std::string::npos) return "";
	p = t.find('"', p + key.size());
	if (p == std::string::npos) return "";
	size_t q = t.find('"', p + 1);
	if (q == std::string::npos) return "";
	std::string v = t.substr(p + 1, q - p - 1);
	for (char c : v) if (!std::isdigit((unsigned char)c)) return "";
	return v;
}

static std::string short_hash(const std::string &h) { return h.size() > 12 ? h.substr(0, 12) + "..." : (h.empty() ? "?" : h); }

Result match_pins(const std::vector<Pin> &pins, const Observed &o) {
	Result r;
	if (pins.empty()) { r.reason = "no version pins configured (refusing to load)"; r.found = "no pins file"; return r; }
	r.expected_build = pins.front().steam_buildid.empty() ? pins.front().name : pins.front().steam_buildid;
	std::string found = "engine " + (o.engine.empty() ? std::string("?") : o.engine);
	if (!o.steam_buildid.empty()) found = "build " + o.steam_buildid + ", " + found;
	found += ", pck " + (o.pck_size < 0 ? std::string("missing") : std::to_string(o.pck_size) + " B " + short_hash(lower_hex(o.pck_sha256)));
	r.found = found;
	std::string last;
	for (const auto &p : pins) {
		if (p.pck_size < 0 || p.pck_sha256.empty()) { last = "pin " + p.name + " is incomplete"; continue; }
		if (!p.engine.empty() && p.engine != o.engine) { last = "engine version differs (" + o.engine + " vs " + p.engine + ")"; continue; }
		if (o.pck_size != p.pck_size) { last = "pck size differs"; continue; }
		if (lower_hex(o.pck_sha256) != lower_hex(p.pck_sha256)) { last = "pck SHA256 differs"; continue; }
		if (p.exe_size >= 0 && o.exe_size != p.exe_size) { last = "exe size differs"; continue; }
		if (!p.exe_sha256.empty() && lower_hex(o.exe_sha256) != lower_hex(p.exe_sha256)) { last = "exe SHA256 differs"; continue; }
		if (!p.steam_buildid.empty() && !o.steam_buildid.empty() && p.steam_buildid != o.steam_buildid) { last = "Steam buildid differs"; continue; }
		r.ok = true;
		r.matched = p.name;
		r.reason = "matched pin " + p.name;
		return r;
	}
	r.reason = "game version changed: " + last;
	return r;
}

}
