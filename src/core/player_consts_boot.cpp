
#include "player_consts_boot.hpp"
#include "version_check.hpp"
#include "../common/jvm_common.hpp"
#include "../gate/player_consts.hpp"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/pck_packer.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <cmath>
#include <cstring>

using namespace godot;

namespace jvm::gdc {
static constexpr uint32_t kTokenBits = 8, kTokenMask = (1u << (kTokenBits - 1)) - 1, kByteMask = 0x80;
static uint32_t u32(const uint8_t *p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
static void put32(std::vector<uint8_t> &o, uint32_t v) { uint8_t b[4]; std::memcpy(b, &v, 4); o.insert(o.end(), b, b + 4); }


static bool variant_at(const uint8_t *p, size_t left, size_t &len, double &num) {
	if (left < 4) return false;
	uint32_t h = u32(p), t = h & 0xff;
	bool wide = (h >> 16) & 1;
	size_t f = wide ? 8 : 4, n = 0;
	num = NAN;
	switch (t) {
		case 0: n = 0; break;
		case 1: n = 4; break;
		case 2: n = wide ? 8 : 4; if (left >= 4 + n) { if (wide) { int64_t v; std::memcpy(&v, p + 4, 8); num = double(v); } else { int32_t v; std::memcpy(&v, p + 4, 4); num = v; } } break;
		case 3: n = f; if (left >= 4 + n) { if (wide) { double v; std::memcpy(&v, p + 4, 8); num = v; } else { float v; std::memcpy(&v, p + 4, 4); num = v; } } break;
		case 4: case 21: { if (left < 8) return false; uint32_t s = u32(p + 4); n = 4 + s + ((4 - s % 4) % 4); break; }
		case 5: n = 2 * f; break; case 6: n = 8; break; case 7: n = 4 * f; break; case 8: n = 16; break;
		case 9: n = 3 * f; break; case 10: n = 12; break; case 11: n = 6 * f; break; case 12: n = 4 * f; break;
		case 13: n = 16; break; case 14: n = 4 * f; break; case 15: n = 4 * f; break; case 16: n = 6 * f; break;
		case 17: n = 9 * f; break; case 18: n = 12 * f; break; case 19: n = 16 * f; break; case 20: n = 16; break;
		default: return false;
	}
	len = 4 + n;
	return len <= left;
}

bool parse(const std::vector<uint8_t> &c, uint32_t version, Buffer &b, std::string &why) {
	auto fail = [&](const char *m) { why = m; return false; };
	if (c.size() < 16) return fail("short buffer");
	b = Buffer(); b.version = version;
	uint32_t nid = u32(&c[0]), ncon = u32(&c[4]), nlines = u32(&c[8]), ntok = u32(&c[12]);
	size_t o = 16;
	if (nid > 1000000 || ncon > 1000000 || nlines > 10000000 || ntok > 10000000) return fail("bad counts");
	for (uint32_t i = 0; i < nid; i++) {
		if (o + 4 > c.size()) return fail("ids");
		uint32_t n = u32(&c[o]); o += 4;
		if (o + size_t(n) * 4 > c.size()) return fail("ids");
		std::u32string s;
		for (uint32_t j = 0; j < n; j++) { uint8_t t[4]; for (int k = 0; k < 4; k++) t[k] = c[o + 4 * j + k] ^ 0xb6; s.push_back(char32_t(u32(t))); }
		o += size_t(n) * 4;
		b.ids.push_back(s);
	}
	size_t cs = o;
	b.ncon = ncon;
	for (uint32_t i = 0; i < ncon; i++) {
		size_t len; double num;
		if (!variant_at(&c[o], c.size() - o, len, num)) return fail("constant of an unhandled type");
		b.const_num.push_back(num);
		o += len;
	}
	b.consts_blob.assign(c.begin() + cs, c.begin() + o);
	if (o + size_t(nlines) * 16 > c.size()) return fail("line map");
	for (uint32_t i = 0; i < nlines; i++) b.lines.push_back({ u32(&c[o + 8 * i]), u32(&c[o + 8 * i + 4]) });
	o += size_t(nlines) * 8;
	for (uint32_t i = 0; i < nlines; i++) b.cols.push_back({ u32(&c[o + 8 * i]), u32(&c[o + 8 * i + 4]) });
	o += size_t(nlines) * 8;
	for (uint32_t i = 0; i < ntok; i++) {
		if (o >= c.size()) return fail("tokens");
		Token t;
		if (c[o] & kByteMask) { if (o + 8 > c.size()) return fail("tokens"); t.type = u32(&c[o]) & ~kByteMask; t.line = u32(&c[o + 4]); t.width = 8; o += 8; }
		else { if (o + 5 > c.size()) return fail("tokens"); t.type = c[o]; t.line = u32(&c[o + 1]); t.width = 5; o += 5; }
		b.toks.push_back(t);
	}
	if (o != c.size()) return fail("trailing bytes");
	return true;
}

std::vector<uint8_t> build_contents(const Buffer &b) {
	std::vector<uint8_t> c;
	put32(c, uint32_t(b.ids.size())); put32(c, b.ncon); put32(c, uint32_t(b.lines.size())); put32(c, uint32_t(b.toks.size()));
	for (const auto &s : b.ids) {
		put32(c, uint32_t(s.size()));
		for (char32_t ch : s) { uint8_t t[4]; uint32_t v = uint32_t(ch); std::memcpy(t, &v, 4); for (int k = 0; k < 4; k++) c.push_back(t[k] ^ 0xb6); }
	}
	c.insert(c.end(), b.consts_blob.begin(), b.consts_blob.end());
	for (auto &kv : b.lines) { put32(c, kv.first); put32(c, kv.second); }
	for (auto &kv : b.cols) { put32(c, kv.first); put32(c, kv.second); }
	for (const Token &t : b.toks) {
		if (t.width == 8 || t.type > kTokenMask) { put32(c, t.type | kByteMask); put32(c, t.line); }
		else { c.push_back(uint8_t(t.type)); put32(c, t.line); }
	}
	return c;
}

bool consts_to_static(Buffer &b, const std::vector<std::pair<std::string, double>> &names, std::string &why) {
	auto fail = [&](const std::string &m) { why = m; return false; };
	std::map<std::u32string, uint32_t> idx;
	for (uint32_t i = 0; i < b.ids.size(); i++) idx[b.ids[i]] = i;
	auto u = [](const std::string &s) { return std::u32string(s.begin(), s.end()); };
	if (!idx.count(u("FEEL"))) return fail("no FEEL identifier (game changed?)");

	std::map<uint32_t, int> votes;
	for (auto &t : b.toks) if ((t.type >> kTokenBits) == idx[u("FEEL")] && (t.type >> kTokenBits)) votes[t.type & kTokenMask]++;
	if (votes.empty()) return fail("FEEL never referenced");
	uint32_t IDENT = 0; int best = -1;
	for (auto &kv : votes) if (kv.second > best) { best = kv.second; IDENT = kv.first; }
	std::map<uint32_t, int> lv;
	for (auto &t : b.toks) if ((t.type >> kTokenBits) && (t.type & kTokenMask) != IDENT) lv[t.type & kTokenMask]++;
	uint32_t LITERAL = 0; best = -1;
	for (auto &kv : lv) if (kv.second > best) { best = kv.second; LITERAL = kv.first; }
	auto is_id = [&](const Token &t, uint32_t id) { return (t.type & kTokenMask) == IDENT && (t.type >> kTokenBits) == id; };
	std::map<uint32_t, uint32_t> line_start(b.lines.begin(), b.lines.end());
	int64_t feel = -1;
	for (size_t i = 2; i < b.toks.size(); i++) if (is_id(b.toks[i], idx[u("FEEL")]) && line_start.count(uint32_t(i - 2))) { feel = int64_t(i); break; }
	if (feel < 0) return fail("`static var FEEL` not found");
	const uint32_t STATIC = b.toks[feel - 2].type, VAR = b.toks[feel - 1].type;
	if (STATIC > kTokenMask || VAR > kTokenMask) return fail("static/var are not keywords");
	int64_t CONST = -1;
	std::map<size_t, std::string> at;
	for (auto &nv : names) {
		auto it = idx.find(u(nv.first));
		if (it == idx.end()) return fail("const " + nv.first + " missing (game updated?)");
		std::vector<size_t> d;
		for (size_t i = 1; i < b.toks.size(); i++)
			if (is_id(b.toks[i], it->second) && line_start.count(uint32_t(i - 1)) && b.toks[i - 1].type <= kTokenMask &&
					b.toks[i - 1].type != STATIC && b.toks[i - 1].type != VAR && b.toks[i - 1].type != IDENT) d.push_back(i);
		if (d.size() != 1) return fail("const " + nv.first + ": " + std::to_string(d.size()) + " declarations");
		const Token &kw = b.toks[d[0] - 1];
		if (CONST < 0) CONST = kw.type;
		if (int64_t(kw.type) != CONST) return fail("const " + nv.first + ": unexpected keyword");

		bool found = false;
		for (size_t k = d[0] + 1; k < std::min(b.toks.size(), d[0] + 6); k++) {
			if ((b.toks[k].type & kTokenMask) != LITERAL) continue;
			uint32_t ci = b.toks[k].type >> kTokenBits;
			if (ci < b.const_num.size() && !std::isnan(b.const_num[ci])) {
				double v = b.const_num[ci];
				found = std::fabs(v - nv.second) < 1e-6 || std::fabs(-v - nv.second) < 1e-6;
				break;
			}
		}
		if (!found) return fail("const " + nv.first + ": literal is not the known stock value (game updated?)");
		at[d[0] - 1] = nv.first;
	}

	std::vector<Token> nt; std::vector<uint32_t> remap(b.toks.size());
	for (size_t i = 0; i < b.toks.size(); i++) {
		remap[i] = uint32_t(nt.size());
		if (at.count(i)) {
			nt.push_back({ STATIC, b.toks[i].line, b.toks[feel - 2].width });
			nt.push_back({ VAR, b.toks[i].line, b.toks[feel - 1].width });
		} else nt.push_back(b.toks[i]);
	}
	for (auto &kv : b.lines) kv.first = remap[kv.first];
	for (auto &kv : b.cols) kv.first = remap[kv.first];
	b.toks.swap(nt);
	return true;
}
}

static const char *kPlayerGd = "res://game/player/player.gd";
static const char *kPlayerGdc = "res://game/player/player.gdc";

static std::vector<std::pair<std::string, double>> spec_names() {
	std::vector<std::pair<std::string, double>> v;
	for (const auto &s : jvm::pconsts::specs()) v.push_back({ s.name, s.stock });
	return v;
}

Dictionary JVPlayerConsts::rewrite(const PackedByteArray &gdc) {
	Dictionary r; r["ok"] = false;
	auto fail = [&](const String &m) { r["reason"] = m; return r; };
	if (gdc.size() < 12 || gdc[0] != 'G' || gdc[1] != 'D' || gdc[2] != 'S' || gdc[3] != 'C') return fail("not a GDSC token file");
	const uint8_t *p = gdc.ptr();
	uint32_t ver = jvm::gdc::u32(p + 4), dsize = jvm::gdc::u32(p + 8);
	if (ver != 100 && ver != 101) return fail("tokenizer version " + String::num_int64(ver) + " not known");
	PackedByteArray body = gdc.slice(12);
	PackedByteArray raw = dsize ? body.decompress(dsize, FileAccess::COMPRESSION_ZSTD) : body;
	if (int64_t(raw.size()) != int64_t(dsize ? dsize : body.size())) return fail("decompress failed");
	std::vector<uint8_t> c(raw.ptr(), raw.ptr() + raw.size());
	jvm::gdc::Buffer b; std::string why;
	if (!jvm::gdc::parse(c, ver, b, why)) return fail(String("parse: ") + why.c_str());
	if (jvm::gdc::build_contents(b) != c) return fail("round trip not byte-identical");
	if (!jvm::gdc::consts_to_static(b, spec_names(), why)) return fail(String::utf8(why.c_str()));
	std::vector<uint8_t> nc = jvm::gdc::build_contents(b);
	PackedByteArray ncb; ncb.resize(int64_t(nc.size())); std::memcpy(ncb.ptrw(), nc.data(), nc.size());
	PackedByteArray comp = ncb.compress(FileAccess::COMPRESSION_ZSTD);
	PackedByteArray out; out.resize(12);
	std::memcpy(out.ptrw(), "GDSC", 4);
	std::memcpy(out.ptrw() + 4, &ver, 4);
	uint32_t ns = uint32_t(nc.size()); std::memcpy(out.ptrw() + 8, &ns, 4);
	out.append_array(comp);
	r["ok"] = true; r["bytes"] = out; r["reason"] = "34 consts -> static var";
	return r;
}

static Dictionary &boot_state() { static Dictionary d; return d; }

Dictionary JVPlayerConsts::prepare_at_boot() {
	const uint64_t t0 = Time::get_singleton()->get_ticks_usec();
	Dictionary st; st["mounted"] = false; st["pin"] = "";
	auto done = [&](const String &why, bool ok) {
		st["reason"] = why; st["mounted"] = ok; st["ms"] = double(Time::get_singleton()->get_ticks_usec() - t0) / 1000.0;
		Engine::get_singleton()->set_meta("jvm_player_consts", st);
		if (ok) jvm::log("consts", "player.consts: overlay mounted (" + why + ")"); else jvm::warn("consts", "player.consts off: " + why);
		return st;
	};
	const String cfg = jvm::mods_root().path_join("player_consts.json");
	if (FileAccess::file_exists(cfg)) {
		Variant v = jvm::load_json(cfg);
		if (v.get_type() == Variant::DICTIONARY && Dictionary(v).has("enabled") && !bool(Dictionary(v)["enabled"])) return done("disabled in mods/player_consts.json", false);
	}
	if (ResourceLoader::get_singleton()->has_cached(kPlayerGd)) return done("player.gd was already loaded before the boot step", false);
	Dictionary pin = jvm::verify_game_version();
	if (!bool(pin.get("ok", false))) return done("game version not pinned", false);
	st["pin"] = String(pin.get("matched", ""));
	if (!FileAccess::file_exists(kPlayerGdc)) return done("no compiled player script in this build", false);
	PackedByteArray gdc = FileAccess::get_file_as_bytes(kPlayerGdc);
	Dictionary rw = rewrite(gdc);
	if (!bool(rw["ok"])) return done(String(rw["reason"]), false);
	DirAccess::make_dir_recursive_absolute("user://jv_mods/cache");
	const String tmp = "user://jv_mods/cache/player_consts.gdc", pck = "user://jv_mods/cache/player_consts.pck";
	{
		Ref<FileAccess> f = FileAccess::open(tmp, FileAccess::WRITE);
		if (f.is_null()) return done("cannot write the cache", false);
		f->store_buffer(PackedByteArray(rw["bytes"]));
	}
	Ref<PCKPacker> pk; pk.instantiate();
	if (pk->pck_start(pck) != OK || pk->add_file(kPlayerGdc, ProjectSettings::get_singleton()->globalize_path(tmp)) != OK || pk->flush() != OK)
		return done("cannot pack the overlay", false);
	if (!ProjectSettings::get_singleton()->load_resource_pack(pck, true)) return done("overlay did not mount", false);
	return done("34 consts editable, build " + String(st["pin"]), true);
}

Dictionary JVPlayerConsts::verify() {
	Dictionary st = Engine::get_singleton()->has_meta("jvm_player_consts") ? Dictionary(Engine::get_singleton()->get_meta("jvm_player_consts")) : Dictionary();
	Dictionary r; r["available"] = false; r["mounted"] = bool(st.get("mounted", false)); r["pin"] = st.get("pin", "");
	Ref<Script> s = ResourceLoader::get_singleton()->load(kPlayerGd);
	if (s.is_null()) { r["reason"] = "player script not loaded"; return r; }
	Dictionary cm = s->get_script_constant_map();
	for (const auto &sp : jvm::pconsts::specs()) {
		if (cm.has(sp.name)) { r["reason"] = String(st.get("reason", "")) + (st.is_empty() ? "boot step did not run" : ""); return r; }
		Variant v = s->get(sp.name);
		if ((v.get_type() != Variant::FLOAT && v.get_type() != Variant::INT) || std::fabs(double(v) - sp.stock) > 1e-6) {
			r["reason"] = String("player script value ") + sp.name + " is not the stock literal";
			return r;
		}
	}
	if (String(r["pin"]).is_empty()) { Dictionary pin = jvm::verify_game_version(); r["pin"] = String(pin.get("matched", "")); }
	r["available"] = true; r["reason"] = "ok";
	return r;
}

void JVPlayerConsts::_bind_methods() {
	ClassDB::bind_method(D_METHOD("prepare_at_boot"), &JVPlayerConsts::prepare_at_boot);
	ClassDB::bind_method(D_METHOD("check"), &JVPlayerConsts::check);
	ClassDB::bind_static_method("JVPlayerConsts", D_METHOD("verify"), &JVPlayerConsts::verify);
	ClassDB::bind_static_method("JVPlayerConsts", D_METHOD("rewrite", "gdc"), &JVPlayerConsts::rewrite);
}
