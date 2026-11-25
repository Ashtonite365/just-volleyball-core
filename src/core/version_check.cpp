#include "version_check.hpp"
#include "../common/jvm_common.hpp"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/hashing_context.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>

using namespace godot;

namespace jvm {

static std::string S(const String &s) { return std::string(s.utf8().get_data()); }

static String sha256_file(const String &path) {
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
	if (f.is_null()) return "";
	Ref<HashingContext> h; h.instantiate();
	h->start(HashingContext::HASH_SHA256);
	const int64_t chunk = 4 * 1024 * 1024;
	while (f->get_position() < f->get_length()) h->update(f->get_buffer(chunk));
	return h->finish().hex_encode();
}


static String cached_sha(const String &path, int64_t size, Dictionary &cache, bool &was_cached, bool &dirty) {
	uint64_t mtime = FileAccess::get_modified_time(path);
	if (cache.has(path)) {
		Dictionary e = cache[path];
		if (int64_t(e.get("size", -1)) == size && int64_t(e.get("mtime", -1)) == int64_t(mtime)) { was_cached = was_cached && true; return e.get("sha256", ""); }
	}
	was_cached = false;
	String h = sha256_file(path);
	Dictionary e; e["size"] = size; e["mtime"] = int64_t(mtime); e["sha256"] = h;
	cache[path] = e;
	dirty = true;
	return h;
}

Dictionary verify_game_version() {
	OS *os = OS::get_singleton();
	const bool dbg = os->is_debug_build();
	String exe = os->get_executable_path();
	String dir = exe.get_base_dir();
	String pck = dir.path_join(exe.get_file().get_basename() + ".pck");
	String pins_path = jvm::install_dir().path_join("pins.json");
	if (dbg) {
		if (!os->get_environment("JV_EXE_PATH").is_empty()) exe = os->get_environment("JV_EXE_PATH");
		if (!os->get_environment("JV_PCK_PATH").is_empty()) pck = os->get_environment("JV_PCK_PATH");
		if (!os->get_environment("JV_PINS_FILE").is_empty()) pins_path = os->get_environment("JV_PINS_FILE");
	}
	const uint64_t t0 = Time::get_singleton()->get_ticks_msec();

	pin::Observed ob;
	ob.engine = S(String(Engine::get_singleton()->get_version_info().get("string", "")));
	Dictionary cache;
	Variant cv = load_json("user://jv_mods/pin_cache.json");
	if (cv.get_type() == Variant::DICTIONARY) cache = cv;
	bool cached = true, dirty = false;
	if (FileAccess::file_exists(pck)) {
		Ref<FileAccess> f = FileAccess::open(pck, FileAccess::READ);
		ob.pck_size = f.is_valid() ? int64_t(f->get_length()) : -1;
		f.unref();
		if (ob.pck_size >= 0) ob.pck_sha256 = S(cached_sha(pck, ob.pck_size, cache, cached, dirty));
	}
	if (FileAccess::file_exists(exe)) {
		Ref<FileAccess> f = FileAccess::open(exe, FileAccess::READ);
		ob.exe_size = f.is_valid() ? int64_t(f->get_length()) : -1;
		f.unref();
		if (ob.exe_size >= 0) ob.exe_sha256 = S(cached_sha(exe, ob.exe_size, cache, cached, dirty));
	}

	String acf = dir.get_base_dir().get_base_dir().path_join("appmanifest_5037200.acf");
	if (FileAccess::file_exists(acf)) ob.steam_buildid = pin::parse_acf_buildid(S(FileAccess::get_file_as_string(acf)));
	if (dirty) {
		DirAccess::make_dir_recursive_absolute("user://jv_mods");
		Ref<FileAccess> w = FileAccess::open("user://jv_mods/pin_cache.json", FileAccess::WRITE);
		if (w.is_valid()) w->store_string(JSON::stringify(cache, "  "));
	}

	std::vector<pin::Pin> pins;
	Variant pv = load_json(pins_path);
	if (pv.get_type() == Variant::DICTIONARY) {
		Array arr = Dictionary(pv).get("pins", Array());
		for (int i = 0; i < arr.size(); i++) {
			if (arr[i].get_type() != Variant::DICTIONARY) continue;
			Dictionary d = arr[i];
			pin::Pin p;
			p.name = S(String(d.get("name", "pin")));
			p.steam_buildid = S(String(d.get("steam_buildid", "")));
			p.engine = S(String(d.get("engine", "")));
			p.pck_size = int64_t(d.get("pck_size", -1));
			p.pck_sha256 = S(String(d.get("pck_sha256", "")));
			p.exe_size = int64_t(d.get("exe_size", -1));
			p.exe_sha256 = S(String(d.get("exe_sha256", "")));
			pins.push_back(p);
		}
	}
	pin::Result r = pin::match_pins(pins, ob);
	const uint64_t ms = Time::get_singleton()->get_ticks_msec() - t0;

	Dictionary out;
	out["ok"] = r.ok;
	out["matched"] = String(r.matched.c_str());
	out["reason"] = String(r.reason.c_str());
	out["found"] = String(r.found.c_str());
	out["expected_build"] = String(r.expected_build.c_str());
	out["engine"] = String(ob.engine.c_str());
	out["pck_path"] = pck;
	out["pck_size"] = ob.pck_size;
	out["pck_sha256"] = String(ob.pck_sha256.c_str());
	out["exe_size"] = ob.exe_size;
	out["steam_buildid"] = String(ob.steam_buildid.c_str());
	out["pins_file"] = pins_path;
	out["hash_ms"] = int64_t(ms);
	out["cached"] = cached && !dirty;
	return out;
}

}
