#include "version_pin.hpp"
#include <cctype>
#include <set>

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

std::string parse_source_const_string(const std::string &src, const std::string &name) {
	if (name.empty()) return "";
	const std::string key = "const " + name;
	size_t p = 0;
	while ((p = src.find(key, p)) != std::string::npos) {
		if (p > 0) {
			unsigned char c = (unsigned char)src[p - 1];
			if (std::isalnum(c) || c == '_') { p += key.size(); continue; }
		}
		size_t i = p + key.size();
		auto skip_ws = [&]() { while (i < src.size() && std::isspace((unsigned char)src[i])) i++; };
		skip_ws();
		if (i < src.size() && src[i] == ':') {
			i++;
			while (i < src.size() && (std::isspace((unsigned char)src[i]) || std::isalnum((unsigned char)src[i]) || src[i] == '_' || src[i] == '.')) i++;
		}
		skip_ws();
		if (i < src.size() && src[i] == '=') {
			i++;
			skip_ws();
			if (i < src.size() && src[i] == '"') {
				size_t j = src.find('"', i + 1);
				if (j != std::string::npos) return src.substr(i + 1, j - i - 1);
			}
		}
		p += key.size();
	}
	return "";
}

static std::string short_hash(const std::string &h) { return h.size() > 12 ? h.substr(0, 12) + "..." : (h.empty() ? "?" : h); }

static std::string describe(const Observed &o) {
	std::string found;
	if (!o.game_version.empty()) found = "version " + o.game_version;
	else found = "version unreadable";
	if (!o.steam_buildid.empty()) found += ", build " + o.steam_buildid;
	found += ", engine " + (o.engine.empty() ? std::string("?") : o.engine);
	found += ", pck " + (o.pck_size < 0 ? std::string("missing") : std::to_string(o.pck_size) + " B " + short_hash(lower_hex(o.pck_sha256)));
	return found;
}

static bool version_mode(const PinFile &file) {
	if (!file.versions.empty()) return true;
	for (const auto &p : file.pins) if (!p.version.empty()) return true;
	return false;
}

static std::set<std::string> approved_versions(const PinFile &file) {
	std::set<std::string> s(file.versions.begin(), file.versions.end());
	for (const auto &p : file.pins) if (!p.version.empty()) s.insert(p.version);
	return s;
}

Result match_pins(const PinFile &file, const Observed &o) {
	Result r;
	r.found = describe(o);
	if (file.pins.empty() && file.versions.empty()) {
		r.reason = "no version pins configured (refusing to load)";
		r.found = "no pins file";
		return r;
	}
	const bool vmode = version_mode(file);
	if (vmode) {
		auto appr = approved_versions(file);
		r.expected_build = appr.empty() ? std::string("?") : *appr.begin();
	} else if (!file.pins.empty()) {
		r.expected_build = file.pins.front().steam_buildid.empty() ? file.pins.front().name : file.pins.front().steam_buildid;
	}

	if (o.game_version.empty()) {
		r.reason = "game version string unreadable";
		return r;
	}
	if (vmode) {
		auto appr = approved_versions(file);
		if (appr.count(o.game_version)) {
			r.ok = true;
			r.expected_build = o.game_version;
			r.matched = "version " + o.game_version;
			for (const auto &p : file.pins) {
				if (p.version == o.game_version) { r.matched = p.name.empty() ? r.matched : p.name; break; }
			}
			r.reason = "matched " + r.matched;
			return r;
		}
		r.reason = "unknown game version (" + o.game_version + ")";
		return r;
	}



	std::string last;
	for (const auto &p : file.pins) {
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

Result match_pins(const std::vector<Pin> &pins, const Observed &obs) {
	PinFile f;
	f.pins = pins;
	return match_pins(f, obs);
}

}
