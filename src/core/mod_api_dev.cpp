


#include "mod_api.hpp"
#include "devtools.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

static JVDevTools *dev() { return JVDevTools::get(); }

Dictionary JVModAPI::profiler_start(const String &mod, double seconds, const String &tag) {
	if (!use(mod, "dev.profiler") || !dev()) { Dictionary r; r["ok"] = false; r["reason"] = dev() ? "refused (dev.profiler)" : "devtools not loaded"; return r; }
	return dev()->profiler_start(seconds, tag);
}
Dictionary JVModAPI::profiler_toggle(const String &mod, double seconds, const String &tag) {

	if (!dev() || !use(mod, "dev.profiler", !bool(dev()->profiler_status().get("capturing", false)))) { Dictionary r; r["ok"] = false; r["reason"] = dev() ? "refused (dev.profiler)" : "devtools not loaded"; return r; }
	return dev()->profiler_toggle(seconds, tag);
}
Dictionary JVModAPI::profiler_stop(const String &mod) { return use(mod, "dev.profiler", false) && dev() ? dev()->profiler_stop() : Dictionary(); }
Dictionary JVModAPI::profiler_status(const String &mod) { return use(mod, "dev.profiler") && dev() ? dev()->profiler_status() : Dictionary(); }
Dictionary JVModAPI::profiler_last_capture(const String &mod, bool frames) { return use(mod, "dev.profiler") && dev() ? dev()->profiler_last(frames) : Dictionary(); }
Dictionary JVModAPI::profiler_summary(const String &mod) { return use(mod, "dev.profiler") && dev() ? dev()->profiler_summary() : Dictionary(); }
Array JVModAPI::profiler_worst_frames(const String &mod, int64_t n) { return use(mod, "dev.profiler") && dev() ? dev()->profiler_worst(n) : Array(); }
Dictionary JVModAPI::profiler_hitches(const String &mod) { return use(mod, "dev.profiler") && dev() ? dev()->profiler_hitches() : Dictionary(); }

bool JVModAPI::debug_set_enabled(const String &mod, bool on) {
	if (!use(mod, "dev.debugger", on) || !dev()) return false;
	dev()->debug_set_enabled(on);
	return true;
}
bool JVModAPI::debug_is_enabled(const String &mod) { return use(mod, "dev.debugger") && dev() && dev()->debug_enabled(); }
bool JVModAPI::debug_set_log_level(const String &mod, const String &tag, int64_t level) { return use(mod, "dev.debugger", level < 0) && dev() && dev()->debug_set_log_level(tag, level); }
Dictionary JVModAPI::debug_log_levels(const String &mod) { return use(mod, "dev.debugger") && dev() ? dev()->debug_log_levels() : Dictionary(); }
Dictionary JVModAPI::debug_capabilities(const String &mod) { return use(mod, "dev.debugger") && dev() ? dev()->debug_capabilities() : Dictionary(); }
Variant JVModAPI::debug_tree(const String &mod, const String &path, int64_t depth) { return use(mod, "dev.debugger") && dev() ? dev()->debug_tree(path, depth) : Variant(); }
Dictionary JVModAPI::debug_node(const String &mod, const String &path) { return use(mod, "dev.debugger") && dev() ? dev()->debug_node(path) : Dictionary(); }
Array JVModAPI::debug_trace(const String &mod, int64_t n) { return use(mod, "dev.debugger") && dev() ? dev()->debug_trace(n) : Array(); }
bool JVModAPI::debug_trace_clear(const String &mod) {
	if (!use(mod, "dev.debugger", false) || !dev()) return false;
	dev()->debug_trace_clear();
	return true;
}
String JVModAPI::debug_dump(const String &mod) { return use(mod, "dev.debugger") && dev() ? dev()->debug_dump() : String(); }
bool JVModAPI::debug_console_toggle(const String &mod) {
	if (!use(mod, "dev.debugger") || !dev()) return false;
	dev()->console_toggle();
	return true;
}

Dictionary JVModAPI::consts_live(const String &mod) { return use(mod, "player.consts_check") && dev() ? dev()->consts_live() : Dictionary(); }
Dictionary JVModAPI::consts_status(const String &mod) { return use(mod, "player.consts_check") && dev() ? dev()->consts_status() : Dictionary(); }
Dictionary JVModAPI::consts_check_write(const String &mod, const String &name, const Variant &value) {

	if (!use(mod, "player.consts_check") || !dev()) { Dictionary r; r["ok"] = false; r["findings"] = Array(); return r; }
	return dev()->consts_check_write(mod, name, value);
}
Dictionary JVModAPI::consts_audit(const String &mod) { return use(mod, "player.consts_check") && dev() ? dev()->consts_audit() : Dictionary(); }
Array JVModAPI::consts_findings(const String &mod) { return use(mod, "player.consts_check") && dev() ? dev()->consts_findings() : Array(); }
bool JVModAPI::consts_clear_findings(const String &mod) {
	if (!use(mod, "player.consts_check", false) || !dev()) return false;
	dev()->consts_clear();
	return true;
}
bool JVModAPI::consts_set_host_values(const String &mod, const Dictionary &values) { return use(mod, "player.consts_check") && dev() && dev()->consts_set_host_values(values); }

void JVModAPI::bind_dev_methods() {
#define BM(name, ...) ClassDB::bind_method(D_METHOD(#name, ##__VA_ARGS__), &JVModAPI::name)
	ClassDB::bind_method(D_METHOD("profiler_start", "mod", "seconds", "tag"), &JVModAPI::profiler_start, DEFVAL(2.0), DEFVAL(""));
	BM(profiler_stop, "mod"); BM(profiler_status, "mod");
	ClassDB::bind_method(D_METHOD("profiler_toggle", "mod", "seconds", "tag"), &JVModAPI::profiler_toggle, DEFVAL(0.0), DEFVAL("api"));
	ClassDB::bind_method(D_METHOD("profiler_last_capture", "mod", "frames"), &JVModAPI::profiler_last_capture, DEFVAL(false));
	BM(profiler_summary, "mod");
	ClassDB::bind_method(D_METHOD("profiler_worst_frames", "mod", "n"), &JVModAPI::profiler_worst_frames, DEFVAL(5));
	BM(profiler_hitches, "mod");
	BM(debug_set_enabled, "mod", "on"); BM(debug_is_enabled, "mod"); BM(debug_set_log_level, "mod", "tag", "level"); BM(debug_log_levels, "mod");
	BM(debug_capabilities, "mod");
	ClassDB::bind_method(D_METHOD("debug_tree", "mod", "path", "depth"), &JVModAPI::debug_tree, DEFVAL(""), DEFVAL(1));
	BM(debug_node, "mod", "path");
	ClassDB::bind_method(D_METHOD("debug_trace", "mod", "n"), &JVModAPI::debug_trace, DEFVAL(64));
	BM(debug_trace_clear, "mod"); BM(debug_dump, "mod"); BM(debug_console_toggle, "mod");
	BM(consts_live, "mod"); BM(consts_status, "mod"); BM(consts_check_write, "mod", "name", "value"); BM(consts_audit, "mod");
	BM(consts_findings, "mod"); BM(consts_clear_findings, "mod"); BM(consts_set_host_values, "mod", "values");
#undef BM
}
