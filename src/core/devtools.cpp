
#include "devtools.hpp"
#include "mod_api.hpp"
#include "../common/jv_prof.hpp"
#include "../common/jvm_common.hpp"

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <climits>
#include <cmath>
#include <set>

using namespace godot;
namespace C = jvm::consts;

JVDevTools *JVDevTools::singleton_ = nullptr;

static const char *kLibNodes[][2] = { { "loader", "/root/JVTextureLoader" }, { "shots", "/root/JVExtraShots" }, { "goofy", "/root/JVGoofyMods" } };
static const char *kPlayerScript = "res://game/player/player.gd";
static inline double ms(uint64_t a, uint64_t b) { return b > a ? double(b - a) / 1e6 : 0.0; }
static std::string S(const String &s) { return std::string(s.utf8().get_data()); }
static String G(const std::string &s) { return String::utf8(s.c_str()); }

void JVDevTools::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_pre_draw"), &JVDevTools::_on_pre_draw);
	ClassDB::bind_method(D_METHOD("_on_post_draw"), &JVDevTools::_on_post_draw);
	ClassDB::bind_method(D_METHOD("_on_console_submit", "text"), &JVDevTools::_on_console_submit);
	ClassDB::bind_method(D_METHOD("profiler_toggle", "seconds", "tag"), &JVDevTools::profiler_toggle, DEFVAL(0.0), DEFVAL("menu"));
	ClassDB::bind_method(D_METHOD("profiler_status"), &JVDevTools::profiler_status);
	ClassDB::bind_method(D_METHOD("set_hotkeys", "on"), &JVDevTools::set_hotkeys);
	ClassDB::bind_method(D_METHOD("hotkeys_enabled"), &JVDevTools::hotkeys_enabled);
}

JVModAPI *JVDevTools::api() const {
	if (!is_inside_tree()) return nullptr;
	return Object::cast_to<JVModAPI>(get_tree()->get_root()->get_node_or_null(NodePath("JVModGate/JVModAPI")));
}

void JVDevTools::load_cfg() {
	if (cfg_loaded_) return;
	cfg_loaded_ = true;
	Dictionary cfg = jvm::load_category_config("core");
	Variant dv = cfg.get("devtools", Variant());
	if (dv.get_type() != Variant::DICTIONARY) return;
	Dictionary d = dv;
	Variant hk = d.get("hotkeys", "auto");
	if (hk.get_type() == Variant::BOOL) { hotkeys_ = bool(hk); hotkeys_source_ = "config"; }
	else { hotkeys_ = kDevBuild; hotkeys_source_ = "build"; }
	overlay_cfg_ = bool(d.get("overlay", true));
	consts_audit_ = bool(d.get("consts_audit", true));
	profiler_key_ = String(d.get("profiler_key", profiler_key_));
	console_key_ = String(d.get("console_key", console_key_));
	if (key_from_name(profiler_key_) == KEY_NONE) { jvm::warn("devtools", "devtools.profiler_key \"" + profiler_key_ + "\" is not a key name; using F6"); profiler_key_ = "F6"; }
	if (key_from_name(console_key_) == KEY_NONE) { jvm::warn("devtools", "devtools.console_key \"" + console_key_ + "\" is not a key name; using F11"); console_key_ = "F11"; }
	double s = double(d.get("profiler_seconds", 2.0));
	default_seconds_ = std::clamp(s, 0.25, 30.0);
	if (bool(d.get("debugger", false))) dbg_on_ = true;
	Variant lv = d.get("log_levels", Variant());
	if (lv.get_type() == Variant::DICTIONARY) {
		Dictionary l = lv;
		Array k = l.keys();
		for (int i = 0; i < k.size(); i++) debug_set_log_level(String(k[i]), int64_t(l[k[i]]));
	}
}

void JVDevTools::_ready() {
	if (role_ == Tail) {
		set_process_priority(INT_MAX);
		set_physics_process_priority(INT_MAX);
		set_process(false);
		set_physics_process(false);
		return;
	}
	singleton_ = this;
	load_cfg();
	set_process_priority(INT_MIN + 1);
	set_physics_process_priority(INT_MIN + 1);
	set_process(false);
	set_physics_process(false);
	set_process_input(hotkeys_);
	JVDevTools *t = memnew(JVDevTools);
	t->setup(Tail, this);
	t->set_name("tail");
	add_child(t);
	tail_ = t;
	jvm::log("devtools", String("ready (profiler idle; debugger ") + (dbg_on_ ? "ON" : "off") + "; hotkeys " + (hotkeys_ ? profiler_key_ + "/" + console_key_ : String("off")) + ")");
}

void JVDevTools::_exit_tree() {
	if (role_ != Head) return;
	if (capturing_) {
		capturing_ = false;
		jvm::prof::tap("core", "end", Variant());
		for (auto &l : kLibNodes) if (Node *n = get_node_or_null(NodePath(l[1]))) if (n->has_method("_jv_dev_tap")) n->call("_jv_dev_tap", "end", Variant());
	}
	if (singleton_ == this) singleton_ = nullptr;
}

void JVDevTools::_notification(int what) {}


Dictionary JVDevTools::profiler_start(double seconds, const String &tag) {
	Dictionary r;
	if (role_ != Head) return host_ ? host_->profiler_start(seconds, tag) : r;
	if (capturing_) { r["ok"] = false; r["reason"] = "a capture is already running"; return r; }
	if (!is_inside_tree()) { r["ok"] = false; r["reason"] = "not in tree"; return r; }
	if (!(seconds > 0)) seconds = default_seconds_;
	cap_seconds_ = std::clamp(seconds, 0.1, 30.0);
	cap_tag_ = tag;
	cap_started_ = Time::get_singleton()->get_datetime_string_from_system(false, true);
	frames_.clear();
	frames_.reserve(size_t(cap_seconds_ * 1000) + 64);
	pending_phys_.clear();
	pending_iter_ = phys_open_ = 0;
	cap_t0_ = 0;
	cap_stop_ = 0;

	jvm::prof::tap("core", "begin", 262144);
	for (auto &l : kLibNodes)
		if (Node *n = get_node_or_null(NodePath(l[1]))) if (n->has_method("_jv_dev_tap")) n->call("_jv_dev_tap", "begin", 262144);

	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs->is_connected("frame_pre_draw", Callable(this, "_on_pre_draw"))) rs->connect("frame_pre_draw", Callable(this, "_on_pre_draw"));
	if (!rs->is_connected("frame_post_draw", Callable(this, "_on_post_draw"))) rs->connect("frame_post_draw", Callable(this, "_on_post_draw"));
	rs->viewport_set_measure_render_time(get_viewport()->get_viewport_rid(), true);
	TypedArray<Node> svs = get_tree()->get_root()->find_children("*", "SubViewport", true, false);
	for (int i = 0; i < svs.size(); i++) if (SubViewport *sv = Object::cast_to<SubViewport>(svs[i])) rs->viewport_set_measure_render_time(sv->get_viewport_rid(), true);
	measuring_vp_ = true;
	capturing_ = true;
	set_process(true);
	set_physics_process(true);
	if (tail_) { tail_->set_process(true); tail_->set_physics_process(true); }
	if (cap_overlay_ || overlay_cfg_ && hotkeys_) show_overlay("profiling " + String::num(cap_seconds_, 2) + " s" + (tag.is_empty() ? String() : " [" + tag + "]"), 1e9);
	trace("profiler", "start", String::num(cap_seconds_, 2) + " s " + tag);
	jvm::log("devtools", "profiler: capture " + String::num(cap_seconds_, 2) + " s started" + (tag.is_empty() ? String() : " [" + tag + "]"));
	r["ok"] = true;
	r["seconds"] = cap_seconds_;
	r["tag"] = tag;
	return r;
}

void JVDevTools::begin_frame(uint64_t now) {
	Frame f;
	f.start = pending_iter_ ? pending_iter_ : now;
	f.proc_head = now;
	f.phys.swap(pending_phys_);
	pending_iter_ = 0;
	frames_.push_back(std::move(f));
}

void JVDevTools::_process(double delta) {
	if (role_ == Tail) {
		if (host_ && host_->capturing_ && !host_->frames_.empty()) host_->frames_.back().proc_tail = jvm::prof::now_ns();
		return;
	}
	if (!capturing_) {

		if (ov_hide_t_ > 0) { ov_hide_t_ -= delta; if (ov_hide_t_ <= 0 && ov_layer_) { ov_layer_->set_visible(false); set_process(false); } }
		else set_process(false);
		return;
	}
	const uint64_t now = jvm::prof::now_ns();

	if (!frames_.empty()) {
		Frame &p = frames_.back();
		Performance *pf = Performance::get_singleton();
		p.eng_proc = pf->get_monitor(Performance::TIME_PROCESS) * 1000.0;
		p.eng_phys = pf->get_monitor(Performance::TIME_PHYSICS_PROCESS) * 1000.0;
		p.draw_calls = int64_t(pf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME));
		p.objects = int64_t(pf->get_monitor(Performance::RENDER_TOTAL_OBJECTS_IN_FRAME));
		p.nodes = int64_t(pf->get_monitor(Performance::OBJECT_NODE_COUNT));
		RenderingServer *rs = RenderingServer::get_singleton();
		double cpu = rs->viewport_get_measured_render_time_cpu(get_viewport()->get_viewport_rid());
		double gpu = rs->viewport_get_measured_render_time_gpu(get_viewport()->get_viewport_rid());
		p.vp_cpu = cpu;
		p.vp_gpu = gpu;
		p.setup_cpu = rs->get_frame_setup_time_cpu();
	}
	if (cap_t0_ == 0) cap_t0_ = pending_iter_ ? pending_iter_ : now;
	if (double(now - cap_t0_) / 1e9 >= cap_seconds_) {
		cap_stop_ = pending_iter_ ? pending_iter_ : now;
		finish_capture();
		return;
	}
	begin_frame(now);
}

void JVDevTools::_physics_process(double) {
	if (role_ == Tail) {
		if (host_ && host_->capturing_ && host_->phys_open_) {
			host_->pending_phys_.push_back({ host_->phys_open_, jvm::prof::now_ns() });
			host_->phys_open_ = 0;
		}
		return;
	}
	if (!capturing_) return;
	const uint64_t now = jvm::prof::now_ns();
	if (!pending_iter_) pending_iter_ = now;
	phys_open_ = now;
}

void JVDevTools::_on_pre_draw() {
	if (capturing_ && !frames_.empty()) frames_.back().pre_draw = jvm::prof::now_ns();
}
void JVDevTools::_on_post_draw() {
	if (capturing_ && !frames_.empty()) frames_.back().post_draw = jvm::prof::now_ns();
}

Dictionary JVDevTools::profiler_stop() {
	if (role_ != Head) return host_ ? host_->profiler_stop() : Dictionary();
	if (!capturing_) { Dictionary r; r["ok"] = false; r["reason"] = "no capture running"; return r; }
	cap_stop_ = jvm::prof::now_ns();
	finish_capture();
	return profiler_last(false);
}

void JVDevTools::finish_capture() {
	capturing_ = false;
	std::vector<Dictionary> taps;
	jvm::prof::tap("core", "end", Variant());
	taps.push_back(jvm::prof::tap("core", "collect", Variant()));
	for (auto &l : kLibNodes) {
		Node *n = get_node_or_null(NodePath(l[1]));
		if (!n || !n->has_method("_jv_dev_tap")) continue;
		n->call("_jv_dev_tap", "end", Variant());
		Variant c = n->call("_jv_dev_tap", "collect", Variant());
		if (c.get_type() == Variant::DICTIONARY) taps.push_back(c);
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs->is_connected("frame_pre_draw", Callable(this, "_on_pre_draw"))) rs->disconnect("frame_pre_draw", Callable(this, "_on_pre_draw"));
	if (rs->is_connected("frame_post_draw", Callable(this, "_on_post_draw"))) rs->disconnect("frame_post_draw", Callable(this, "_on_post_draw"));
	if (measuring_vp_) {
		rs->viewport_set_measure_render_time(get_viewport()->get_viewport_rid(), false);
		TypedArray<Node> svs = get_tree()->get_root()->find_children("*", "SubViewport", true, false);
		for (int i = 0; i < svs.size(); i++) if (SubViewport *sv = Object::cast_to<SubViewport>(svs[i])) rs->viewport_set_measure_render_time(sv->get_viewport_rid(), false);
		measuring_vp_ = false;
	}
	set_physics_process(false);
	if (tail_) { tail_->set_process(false); tail_->set_physics_process(false); }
	last_ = build_capture(taps, cap_stop_ ? cap_stop_ : jvm::prof::now_ns());
	frames_.clear();
	frames_.shrink_to_fit();
	pending_phys_.clear();
	write_capture(last_);
	captures_++;
	Dictionary sm = last_.get("meta", Dictionary());
	String txt = "capture: " + String::num_int64(int64_t(sm.get("frames", 0))) + " frames, avg " + String::num(double(sm.get("avg_frame_ms", 0.0)), 2) + " ms, worst " + String::num(double(sm.get("worst_frame_ms", 0.0)), 1) + " ms";
	Array hz = Dictionary(last_.get("hitches", Dictionary())).get("list", Array());
	if (hz.size()) txt += ", " + String::num_int64(hz.size()) + " hitches";
	trace("profiler", "done", txt);
	jvm::log("devtools", "profiler: " + txt + " -> " + last_json_);
	if (ov_layer_ && ov_layer_->is_visible()) show_overlay(txt, 8.0);
	else set_process(false);
	cap_overlay_ = false;
}

namespace {
struct Stat { double mean = 0, p50 = 0, p95 = 0, max = 0, total = 0; int64_t nonzero = 0; };
Stat stat_of(std::vector<double> v) {
	Stat s;
	if (v.empty()) return s;
	for (double x : v) { s.total += x; if (x > 0) s.nonzero++; s.max = std::max(s.max, x); }
	s.mean = s.total / double(v.size());
	std::sort(v.begin(), v.end());
	s.p50 = v[v.size() / 2];
	s.p95 = v[std::min(v.size() - 1, size_t(std::ceil(0.95 * double(v.size()))) - (v.size() > 1 ? 1 : 0))];
	return s;
}
Dictionary stat_dict(const Stat &s, int64_t calls) {
	Dictionary d;
	d["mean_ms"] = s.mean; d["p50_ms"] = s.p50; d["p95_ms"] = s.p95; d["max_ms"] = s.max; d["total_ms"] = s.total;
	d["frames_nonzero"] = s.nonzero;
	if (calls >= 0) d["calls"] = calls;
	return d;
}
}

Dictionary JVDevTools::build_capture(const std::vector<Dictionary> &taps, uint64_t stop_ns) {
	const size_t N = frames_.size();
	std::vector<uint64_t> ends(N);
	for (size_t i = 0; i < N; i++) ends[i] = i + 1 < N ? frames_[i + 1].start : std::max(stop_ns, frames_[i].start);

	std::map<std::string, std::set<std::string>> libs_of;
	for (auto &t : taps) {
		PackedStringArray nm = t.get("names", PackedStringArray());
		PackedInt32Array st = t.get("stage", PackedInt32Array());
		std::set<int> used;
		for (int i = 0; i < st.size(); i++) used.insert(st[i]);
		for (int id : used) if (id >= 0 && id < nm.size()) libs_of[S(nm[id])].insert(S(String(t.get("library", ""))));
	}
	std::vector<std::string> keys;
	std::map<std::string, int> key_ix;
	auto key_for = [&](const std::string &name, const std::string &lib) {
		std::string k = libs_of[name].size() > 1 ? name + "[" + lib + "]" : name;
		auto it = key_ix.find(k);
		if (it != key_ix.end()) return it->second;
		key_ix[k] = int(keys.size());
		keys.push_back(k);
		return int(keys.size() - 1);
	};
	std::vector<std::map<int, double>> fstage(N);
	std::vector<std::map<int, int64_t>> fcalls(N);
	std::vector<double> mods_proc(N, 0), mods_phys(N, 0), mods_other(N, 0);
	std::map<int, int64_t> calls_total;
	Array tapinfo;
	int64_t outside = 0;
	for (auto &t : taps) {
		const std::string lib = S(String(t.get("library", "")));
		PackedStringArray nm = t.get("names", PackedStringArray());
		PackedInt32Array st = t.get("stage", PackedInt32Array());
		PackedInt32Array dp = t.get("depth", PackedInt32Array());
		PackedInt64Array a = t.get("t0_ns", PackedInt64Array());
		PackedInt64Array b = t.get("t1_ns", PackedInt64Array());
		Dictionary ti;
		ti["library"] = G(lib); ti["recorded"] = t.get("recorded", 0); ti["dropped"] = t.get("dropped", 0);
		tapinfo.push_back(ti);
		std::vector<int> idmap(nm.size(), -1);
		for (int i = 0; i < st.size() && i < a.size() && i < b.size(); i++) {
			const int id = st[i];
			if (id < 0 || id >= nm.size()) continue;
			if (idmap[id] < 0) idmap[id] = key_for(S(nm[id]), lib);
			const uint64_t t0 = uint64_t(a[i]), t1 = uint64_t(b[i]);
			if (N == 0 || t0 < frames_[0].start || t0 >= ends[N - 1]) { outside++; continue; }

			size_t lo = 0, hi = N - 1;
			while (lo < hi) { size_t mid = (lo + hi + 1) / 2; if (frames_[mid].start <= t0) lo = mid; else hi = mid - 1; }
			const int k = idmap[id];
			const double d = ms(t0, t1);
			fstage[lo][k] += d;
			fcalls[lo][k]++;
			calls_total[k]++;
			if (i < dp.size() && dp[i] == 0) {
				const Frame &f = frames_[lo];
				bool in_phys = false;
				for (auto &p : f.phys) if (t0 >= p.first && t0 <= p.second) { in_phys = true; break; }
				if (in_phys) mods_phys[lo] += d;
				else if (f.proc_tail && t0 >= f.proc_head && t0 <= f.proc_tail) mods_proc[lo] += d;
				else mods_other[lo] += d;
			}
		}
	}

	static const char *builtin[] = { "frame_ms", "physics_ms", "process_ms", "render_cpu_ms", "other_ms", "game_process_ms", "mods_process_ms",
		"game_physics_ms", "mods_physics_ms", "mods_other_ms", "engine_process_ms", "engine_physics_ms", "viewport_render_cpu_ms", "viewport_render_gpu_ms", "frame_setup_cpu_ms" };
	const int NB = int(sizeof(builtin) / sizeof(builtin[0]));
	std::vector<std::vector<double>> bcol(NB, std::vector<double>(N, 0));
	Array rows;
	for (size_t i = 0; i < N; i++) {
		const Frame &f = frames_[i];
		double phys = 0;
		for (auto &p : f.phys) phys += ms(p.first, p.second);
		const double frame = ms(f.start, ends[i]);
		const double proc = f.proc_tail ? ms(f.proc_head, f.proc_tail) : 0;
		const double rend = (f.pre_draw && f.post_draw) ? ms(f.pre_draw, f.post_draw) : 0;
		double v[NB] = { frame, phys, proc, rend, std::max(0.0, frame - phys - proc - rend), std::max(0.0, proc - mods_proc[i]), mods_proc[i],
			std::max(0.0, phys - mods_phys[i]), mods_phys[i], mods_other[i], std::max(0.0, f.eng_proc), std::max(0.0, f.eng_phys),
			std::max(0.0, f.vp_cpu), std::max(0.0, f.vp_gpu), std::max(0.0, f.setup_cpu) };
		Dictionary r;
		r["i"] = int64_t(i);
		r["t_ms"] = ms(frames_[0].start, f.start);
		for (int b = 0; b < NB; b++) { r[builtin[b]] = v[b]; bcol[b][i] = v[b]; }
		r["physics_ticks"] = int64_t(f.phys.size());
		r["draw_calls"] = f.draw_calls; r["objects"] = f.objects; r["nodes"] = f.nodes;
		Dictionary sd;
		for (auto &[k, d] : fstage[i]) sd[G(keys[k])] = d;
		r["stages"] = sd;
		rows.push_back(r);
	}

	Dictionary summary;
	for (int b = 0; b < NB; b++) summary[builtin[b]] = stat_dict(stat_of(bcol[b]), -1);
	Dictionary mods;
	for (size_t k = 0; k < keys.size(); k++) {
		std::vector<double> col(N, 0);
		for (size_t i = 0; i < N; i++) { auto it = fstage[i].find(int(k)); if (it != fstage[i].end()) col[i] = it->second; }
		mods[G(keys[k])] = stat_dict(stat_of(col), calls_total[int(k)]);
	}

	auto top_of = [&](size_t i, int n) {
		std::vector<std::pair<double, String>> v;
		const char *parts[] = { "physics_ms", "game_process_ms", "render_cpu_ms", "other_ms", "mods_other_ms" };
		const int pix[] = { 1, 5, 3, 4, 9 };
		for (int p = 0; p < 5; p++) v.push_back({ bcol[pix[p]][i], parts[p] });
		for (auto &[k, d] : fstage[i]) v.push_back({ d, G(keys[k]) });
		std::sort(v.begin(), v.end(), [](auto &x, auto &y) { return x.first > y.first; });
		Array out;
		for (int j = 0; j < n && j < int(v.size()); j++) { if (v[j].first <= 0.0005) break; Dictionary e; e["stage"] = v[j].second; e["ms"] = v[j].first; out.push_back(e); }
		return out;
	};
	std::vector<size_t> order(N);
	for (size_t i = 0; i < N; i++) order[i] = i;
	std::sort(order.begin(), order.end(), [&](size_t x, size_t y) { return bcol[0][x] > bcol[0][y]; });
	Array worst;
	for (size_t j = 0; j < order.size() && j < 10; j++) {
		Dictionary w = Dictionary(rows[order[j]]).duplicate();
		w["top"] = top_of(order[j], 8);
		worst.push_back(w);
	}

	Stat fs = stat_of(bcol[0]);
	const double thr = std::max(2.0 * fs.p50, fs.p50 + 8.0);
	Array hl;
	PackedFloat64Array gaps;
	double last_t = -1;
	for (size_t i = 0; i < N; i++) {
		if (bcol[0][i] <= thr) continue;
		Dictionary h;
		const double t = ms(frames_[0].start, frames_[i].start);
		h["i"] = int64_t(i); h["t_ms"] = t; h["frame_ms"] = bcol[0][i]; h["top"] = top_of(i, 6);
		hl.push_back(h);
		if (last_t >= 0) gaps.push_back(t - last_t);
		last_t = t;
	}
	Dictionary hitches;
	hitches["threshold_ms"] = thr;
	hitches["list"] = hl;
	hitches["intervals_ms"] = gaps;
	if (gaps.size()) {
		std::vector<double> g(gaps.ptr(), gaps.ptr() + gaps.size());
		Stat gs = stat_of(g);
		hitches["interval_mean_ms"] = gs.mean;
		hitches["interval_median_ms"] = gs.p50;
	}

	if (hl.size()) {
		std::map<String, std::pair<double, double>> cmp;
		size_t nh = 0, nn = 0;
		for (size_t i = 0; i < N; i++) {
			const bool h = bcol[0][i] > thr;
			(h ? nh : nn)++;
			for (auto &[k, d] : fstage[i]) (h ? cmp[G(keys[k])].first : cmp[G(keys[k])].second) += d;
			const int pix[] = { 1, 5, 3, 4 };
			const char *parts[] = { "physics_ms", "game_process_ms", "render_cpu_ms", "other_ms" };
			for (int p = 0; p < 4; p++) (h ? cmp[parts[p]].first : cmp[parts[p]].second) += bcol[pix[p]][i];
		}
		std::vector<std::pair<double, String>> ex;
		Dictionary per;
		for (auto &[k, v] : cmp) {
			const double hm = nh ? v.first / double(nh) : 0, nm = nn ? v.second / double(nn) : 0;
			Dictionary e; e["hitch_mean_ms"] = hm; e["normal_mean_ms"] = nm; e["extra_ms"] = hm - nm;
			per[k] = e;
			ex.push_back({ hm - nm, k });
		}
		std::sort(ex.begin(), ex.end(), [](auto &x, auto &y) { return x.first > y.first; });
		Array expl;
		for (size_t j = 0; j < ex.size() && j < 10; j++) { if (ex[j].first <= 0.01) break; Dictionary e; e["stage"] = ex[j].second; e["extra_ms"] = ex[j].first; expl.push_back(e); }
		hitches["explained_by"] = expl;
		hitches["per_stage"] = per;
	}

	Dictionary meta;
	meta["tag"] = cap_tag_;
	meta["started"] = cap_started_;
	meta["seconds_requested"] = cap_seconds_;
	meta["seconds"] = N ? ms(frames_[0].start, ends[N - 1]) / 1000.0 : 0.0;
	meta["frames"] = int64_t(N);
	meta["avg_frame_ms"] = fs.mean;
	meta["median_frame_ms"] = fs.p50;
	meta["worst_frame_ms"] = fs.max;
	meta["fps_avg"] = fs.mean > 0 ? 1000.0 / fs.mean : 0.0;
	meta["renderer"] = RenderingServer::get_singleton()->get_current_rendering_driver_name();
	meta["mode"] = api() ? api()->get_mode() : String();
	Node *sc = jvm::current_scene(this);
	meta["scene"] = sc ? sc->get_scene_file_path() : String();
	meta["taps"] = tapinfo;
	meta["samples_outside_frames"] = outside;
	meta["notes"] = "frame = one main-loop iteration from its first physics tick (or first _process) to the next; physics_ms = sum of physics ticks; "
					"process_ms = first to last _process; render_cpu_ms = frame_pre_draw..frame_post_draw; other_ms = the rest (input, audio, swap/vsync, pacing). "
					"game_* = phase minus our top-level mod hooks. engine_* and viewport_render_* come from Performance / RenderingServer for the same frame (GPU time may lag a frame; 0 when the driver does not measure). "
					"Nested stages (a.b inside a) are included in their parent.";
	Dictionary out;
	out["meta"] = meta;
	out["summary"] = summary;
	out["stages"] = mods;
	out["worst"] = worst;
	out["hitches"] = hitches;
	out["frames"] = rows;
	PackedStringArray sk;
	for (auto &k : keys) sk.push_back(G(k));
	out["stage_names"] = sk;
	return out;
}

void JVDevTools::write_capture(Dictionary &cap) {
	DirAccess::make_dir_recursive_absolute("user://jv_profiler");
	String stamp = cap_started_.replace("-", "").replace(":", "").replace(" ", "-").replace("T", "-");
	String tag;
	for (int i = 0; i < cap_tag_.length() && i < 32; i++) {
		char32_t c = cap_tag_[i];
		tag += (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ? String::chr(c) : String("_");
	}
	String base = "user://jv_profiler/capture-" + stamp + "-" + String::num_int64(captures_) + (tag.is_empty() ? String() : "-" + tag);
	last_json_ = base + ".json";
	last_csv_ = base + ".csv";
	{
		Ref<FileAccess> f = FileAccess::open(last_json_, FileAccess::WRITE);
		if (f.is_valid()) { f->store_string(JSON::stringify(cap, " ", false)); f->close(); }
	}
	{
		Ref<FileAccess> f = FileAccess::open(last_csv_, FileAccess::WRITE);
		if (f.is_valid()) {
			static const char *cols[] = { "i", "t_ms", "frame_ms", "physics_ms", "physics_ticks", "process_ms", "game_process_ms", "mods_process_ms", "render_cpu_ms", "other_ms",
				"game_physics_ms", "mods_physics_ms", "mods_other_ms", "engine_process_ms", "engine_physics_ms", "viewport_render_cpu_ms", "viewport_render_gpu_ms", "frame_setup_cpu_ms", "draw_calls", "objects", "nodes" };
			PackedStringArray sk = cap.get("stage_names", PackedStringArray());
			String h;
			for (const char *c : cols) h += String(h.is_empty() ? "" : ",") + c;
			for (int k = 0; k < sk.size(); k++) h += "," + sk[k];
			f->store_line(h);
			Array rows = cap.get("frames", Array());
			for (int i = 0; i < rows.size(); i++) {
				Dictionary r = rows[i];
				String line;
				bool first = true;
				for (const char *c : cols) {
					Variant v = r.get(c, 0);
					line += String(first ? "" : ",") + (v.get_type() == Variant::FLOAT ? String::num(double(v), 4) : String(v));
					first = false;
				}
				Dictionary st = r.get("stages", Dictionary());
				for (int k = 0; k < sk.size(); k++) line += "," + (st.has(sk[k]) ? String::num(double(st[sk[k]]), 4) : String("0"));
				f->store_line(line);
			}
			f->close();
		}
	}
	Dictionary meta = cap.get("meta", Dictionary());
	meta["json"] = last_json_;
	meta["csv"] = last_csv_;
	meta["json_abs"] = ProjectSettings::get_singleton()->globalize_path(last_json_);
	cap["meta"] = meta;
}

Dictionary JVDevTools::profiler_status() const {
	if (role_ != Head && host_) return host_->profiler_status();
	Dictionary d;
	d["capturing"] = capturing_;
	d["seconds"] = cap_seconds_;
	d["elapsed"] = capturing_ && cap_t0_ ? double(jvm::prof::now_ns() - cap_t0_) / 1e9 : 0.0;
	d["frames"] = int64_t(frames_.size());
	d["captures"] = int64_t(captures_);
	d["last_json"] = last_json_;
	d["last_csv"] = last_csv_;
	d["default_seconds"] = default_seconds_;
	d["hotkeys"] = hotkeys_;
	d["profiler_key"] = profiler_key_;
	d["console_key"] = console_key_;
	d["hotkeys_source"] = hotkeys_source_;
	d["dev_build"] = kDevBuild;
	return d;
}

Dictionary JVDevTools::profiler_last(bool frames) const {
	if (frames || last_.is_empty()) return last_;
	Dictionary d = last_.duplicate(false);
	d.erase("frames");
	return d;
}
Dictionary JVDevTools::profiler_summary() const {
	Dictionary d;
	if (last_.is_empty()) return d;
	d["meta"] = last_["meta"];
	d["summary"] = last_["summary"];
	d["stages"] = last_["stages"];
	return d;
}
Array JVDevTools::profiler_worst(int64_t n) const {
	Array w = last_.get("worst", Array());
	Array out;
	for (int i = 0; i < w.size() && i < n; i++) out.push_back(w[i]);
	return out;
}
Dictionary JVDevTools::profiler_hitches() const { return last_.get("hitches", Dictionary()); }

void JVDevTools::show_overlay(const String &text, double hold_s) {
	if (!ov_layer_) {
		ov_layer_ = memnew(CanvasLayer);
		ov_layer_->set_layer(127);
		add_child(ov_layer_);
		ov_label_ = memnew(Label);
		ov_label_->set_position(Vector2(12, 8));
		ov_label_->add_theme_font_size_override("font_size", 14);
		ov_label_->add_theme_color_override("font_color", Color(1, 0.95, 0.4));
		ov_label_->add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.9));
		ov_label_->add_theme_constant_override("outline_size", 4);
		ov_label_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		ov_layer_->add_child(ov_label_);
	}
	ov_label_->set_text("[jv-mods devtools] " + text);
	ov_layer_->set_visible(true);
	ov_hide_t_ = hold_s;
	set_process(true);
}

void JVDevTools::_input(const Ref<InputEvent> &e) {
	if (role_ != Head || !hotkeys_) return;
	Ref<InputEventKey> k = e;
	if (k.is_null() || !k->is_pressed() || k->is_echo()) return;
	OS *os = OS::get_singleton();
	const Key code = k->get_keycode();
	(void)os;
	if (code == key_from_name(profiler_key_)) {
		profiler_toggle(0, "hotkey");
		get_viewport()->set_input_as_handled();
	} else if (code == key_from_name(console_key_)) {
		console_toggle();
		get_viewport()->set_input_as_handled();
	}
}

Key JVDevTools::key_from_name(const String &name_in) {
	String n = name_in.strip_edges();
	if (n.to_upper().begins_with("KEY_")) n = n.substr(4);
	if (n.is_empty()) return KEY_NONE;
	return Key(OS::get_singleton()->find_keycode_from_string(n));
}

void JVDevTools::set_hotkeys(bool on) {
	if (role_ != Head) { if (host_) host_->set_hotkeys(on); return; }
	hotkeys_ = on;
	hotkeys_source_ = "runtime";
	set_process_input(on);
	jvm::log("devtools", String("hotkeys ") + (on ? "on (" + profiler_key_ + " profiler, " + console_key_ + " console)" : String("off")));
}

String JVDevTools::profiler_menu_label() const {
	const JVDevTools *h = role_ == Head ? this : host_;
	if (!h) return "dev tools: frame profiler";
	String key = h->hotkeys_ ? "   [" + h->profiler_key_ + "]" : String();
	if (h->capturing_) return "dev tools: frame profiler   -   capturing (press to stop)" + key;
	return "dev tools: frame profiler   -   capture " + String::num(h->default_seconds_, 2) + " s" + key +
			(h->last_json_.is_empty() ? String() : "   (last: " + h->last_json_.get_file() + ")");
}

Dictionary JVDevTools::profiler_toggle(double seconds, const String &tag) {
	if (role_ != Head) return host_ ? host_->profiler_toggle(seconds, tag) : Dictionary();
	if (capturing_) {
		Dictionary r = profiler_stop();
		r["toggled"] = "stopped";
		return r;
	}
	cap_overlay_ = true;
	Dictionary r = profiler_start(seconds > 0 ? seconds : default_seconds_, tag);
	r["toggled"] = "started";
	return r;
}


void JVDevTools::debug_set_enabled(bool on) {
	if (dbg_on_ == on) return;
	dbg_on_ = on;
	if (!on) { trace_.clear(); if (con_layer_) con_layer_->set_visible(false); }
	jvm::log("devtools", String("debugger ") + (on ? "ON" : "off"));
}

bool JVDevTools::debug_set_log_level(const String &tag, int64_t level) {
	if (tag.is_empty()) return false;
	Engine *e = Engine::get_singleton();
	Dictionary d = e->has_meta("jv_log_levels") ? Dictionary(e->get_meta("jv_log_levels")) : Dictionary();
	d = d.duplicate();
	if (level < 0) d.erase(tag);
	else d[tag] = int64_t(std::min<int64_t>(level, 3));
	if (d.is_empty()) e->remove_meta("jv_log_levels");
	else e->set_meta("jv_log_levels", d);
	trace("debugger", "log_level", tag + String("=") + String::num_int64(level));
	return true;
}

Dictionary JVDevTools::debug_log_levels() const {
	Engine *e = Engine::get_singleton();
	return e->has_meta("jv_log_levels") ? Dictionary(e->get_meta("jv_log_levels")).duplicate() : Dictionary();
}

Dictionary JVDevTools::debug_capabilities() const {
	Dictionary d;
	JVModAPI *a = api();
	if (!a) return d;
	Array caps = a->get_capabilities();
	Dictionary by_status, open_count;
	Array rows;
	for (int i = 0; i < caps.size(); i++) {
		Dictionary c = caps[i];
		String st = c.get("status", "");
		by_status[st] = int64_t(by_status.get(st, 0)) + 1;
		if (bool(c.get("open", false))) open_count[st] = int64_t(open_count.get(st, 0)) + 1;
		Dictionary r;
		r["id"] = c["id"]; r["status"] = st; r["access"] = c.get("access", ""); r["open"] = c.get("open", false);
		rows.push_back(r);
	}
	d["capabilities"] = rows;
	d["by_status"] = by_status;
	d["open_by_status"] = open_count;
	d["mode"] = a->get_mode();
	Dictionary cls;
	for (const char *x : { "read", "local_ui", "cosmetic", "gameplay", "sync", "offline" }) cls[x] = a->is_open(x);
	d["classes"] = cls;
	d["mods"] = a->get_mods();
	Dictionary libs;
	for (const char *l : { "loader", "textures", "models", "shots", "goofy", "menu_ui", "menu_perf", "picker" }) {
		Node *n = a->core_library(l);
		Dictionary e;
		e["present"] = n != nullptr;
		if (n && n->has_method("get_status")) e["status"] = n->call("get_status");
		if (n && n->has_method("_jv_dev_tap")) e["profiler_tap"] = true;
		libs[l] = e;
	}
	d["libraries"] = libs;
	d["api_version"] = a->get_api_version();
	return d;
}

bool JVDevTools::node_is_ours(Node *n) const {
	if (!n || !n->is_inside_tree()) return false;
	Node *root = get_tree()->get_root();
	for (Node *p = n; p && p != root; p = p->get_parent()) {
		String nm = p->get_name();
		if (nm.begins_with("JVM_") || nm.begins_with("jvm_")) return true;
		if (p->get_parent() == root && nm.begins_with("JV")) return true;
	}
	return false;
}

Dictionary JVDevTools::node_info(Node *n, int depth) const {
	Dictionary d;
	d["name"] = String(n->get_name());
	d["path"] = String(n->get_path());
	d["class"] = n->get_class();
	Ref<Script> s = n->get_script();
	if (s.is_valid()) d["script"] = s->get_path();
	if (CanvasItem *ci = Object::cast_to<CanvasItem>(n)) d["visible"] = ci->is_visible();
	if (Node3D *n3 = Object::cast_to<Node3D>(n)) d["visible"] = n3->is_visible();
	d["processing"] = n->is_processing();
	d["physics_processing"] = n->is_physics_processing();
	d["children"] = int64_t(n->get_child_count());
	if (depth > 0) {
		Array kids;
		for (int i = 0; i < n->get_child_count() && i < 64; i++) kids.push_back(node_info(n->get_child(i), depth - 1));
		d["kids"] = kids;
	}
	return d;
}

Variant JVDevTools::debug_tree(const String &path, int64_t depth) const {
	if (!is_inside_tree()) return Variant();
	depth = std::clamp<int64_t>(depth, 0, 6);
	Node *root = get_tree()->get_root();
	if (!path.is_empty()) {
		Node *n = root->get_node_or_null(NodePath(path));
		if (!n || !node_is_ours(n)) return Variant();
		return node_info(n, int(depth));
	}
	Array out;
	for (int i = 0; i < root->get_child_count(); i++) {
		Node *c = root->get_child(i);
		if (String(c->get_name()).begins_with("JV")) out.push_back(node_info(c, int(depth)));
	}

	Node *sc = jvm::current_scene(const_cast<JVDevTools *>(this));
	std::vector<std::pair<Node *, int>> st;
	if (sc) st.push_back({ sc, 0 });
	int visited = 0;
	while (!st.empty() && visited < 6000 && out.size() < 96) {
		auto [n, dd] = st.back();
		st.pop_back();
		visited++;
		String nm = n->get_name();
		if (n != sc && (nm.begins_with("JVM_") || nm.begins_with("jvm_"))) { out.push_back(node_info(n, int(depth))); continue; }
		if (dd >= 16) continue;
		for (int i = n->get_child_count() - 1; i >= 0; i--) st.push_back({ n->get_child(i), dd + 1 });
	}
	return out;
}

Dictionary JVDevTools::debug_node(const String &path) const {
	Dictionary d;
	if (!is_inside_tree()) return d;
	Node *n = get_tree()->get_root()->get_node_or_null(NodePath(path));
	if (!n || !node_is_ours(n)) return d;
	d = node_info(n, 0);
	Dictionary props;
	TypedArray<Dictionary> pl = n->get_property_list();
	int c = 0;
	for (int i = 0; i < pl.size() && c < 200; i++) {
		Dictionary p = pl[i];
		int64_t usage = p.get("usage", 0);
		if (!(usage & (PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_SCRIPT_VARIABLE))) continue;
		String nm = p.get("name", "");
		Variant v = n->get(nm);
		String s = v.get_type() == Variant::OBJECT ? (v.operator Object *() ? String(v.operator Object *()->get_class()) : String("null")) : String(v);
		props[nm] = s.left(160);
		c++;
	}
	d["properties"] = props;
	Array metas;
	TypedArray<StringName> ml = n->get_meta_list();
	for (int i = 0; i < ml.size(); i++) metas.push_back(String(StringName(ml[i])));
	d["meta"] = metas;
	return d;
}

void JVDevTools::trace(const String &kind, const String &name, const String &detail) {
	if (!dbg_on_) return;
	Trace t;
	t.t_ms = double(Time::get_singleton()->get_ticks_usec()) / 1000.0;
	t.frame = int64_t(Engine::get_singleton()->get_process_frames());
	t.kind = kind; t.name = name; t.detail = detail.left(240);
	trace_.push_back(t);
	while (trace_.size() > 512) trace_.pop_front();
	if (con_out_ && con_layer_ && con_layer_->is_visible())
		con_out_->append_text("[color=#8ab4ff]" + String::num(t.t_ms / 1000.0, 2) + "[/color] " + kind + " " + name + " " + t.detail.xml_escape() + "\n");
}

Array JVDevTools::debug_trace(int64_t n) const {
	Array out;
	int64_t start = std::max<int64_t>(0, int64_t(trace_.size()) - std::max<int64_t>(n, 0));
	for (size_t i = size_t(start); i < trace_.size(); i++) {
		const Trace &t = trace_[i];
		Dictionary d;
		d["t_ms"] = t.t_ms; d["frame"] = t.frame; d["kind"] = t.kind; d["name"] = t.name; d["detail"] = t.detail;
		out.push_back(d);
	}
	return out;
}

String JVDevTools::debug_dump() {
	Dictionary d;
	d["capabilities"] = debug_capabilities();
	d["log_levels"] = debug_log_levels();
	d["tree"] = debug_tree("", 2);
	d["trace"] = debug_trace(512);
	d["consts"] = consts_status();
	d["consts_findings"] = consts_findings();
	d["profiler"] = profiler_status();
	DirAccess::make_dir_recursive_absolute("user://jv_profiler");
	String stamp = Time::get_singleton()->get_datetime_string_from_system(false, true).replace("-", "").replace(":", "").replace(" ", "-").replace("T", "-");
	String p = "user://jv_profiler/debug-dump-" + stamp + ".json";
	Ref<FileAccess> f = FileAccess::open(p, FileAccess::WRITE);
	if (f.is_null()) return String();
	f->store_string(JSON::stringify(d, " ", false));
	f->close();
	return p;
}

void JVDevTools::ensure_console() {
	if (con_layer_) return;
	con_layer_ = memnew(CanvasLayer);
	con_layer_->set_layer(126);
	con_layer_->set_visible(false);
	add_child(con_layer_);
	con_panel_ = memnew(PanelContainer);
	con_panel_->set_anchors_preset(Control::PRESET_TOP_WIDE);
	con_panel_->set_offset(SIDE_BOTTOM, 300);
	Ref<StyleBoxFlat> sb;
	sb.instantiate();
	sb->set_bg_color(Color(0.05, 0.06, 0.09, 0.92));
	sb->set_content_margin_all(8);
	con_panel_->add_theme_stylebox_override("panel", sb);
	con_layer_->add_child(con_panel_);
	VBoxContainer *v = memnew(VBoxContainer);
	con_panel_->add_child(v);
	con_out_ = memnew(RichTextLabel);
	con_out_->set_use_bbcode(true);
	con_out_->set_scroll_follow(true);
	con_out_->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	con_out_->add_theme_font_size_override("normal_font_size", 13);
	con_out_->set_selection_enabled(true);
	v->add_child(con_out_);
	con_in_ = memnew(LineEdit);
	con_in_->set_placeholder("jv-mods debug console - type help");
	con_in_->connect("text_submitted", Callable(this, "_on_console_submit"));
	v->add_child(con_in_);
	con_out_->append_text("[b]jv-mods CORE debug console[/b] (type help)\n");
}

void JVDevTools::console_toggle() {
	if (!dbg_on_) debug_set_enabled(true);
	ensure_console();
	const bool show = !con_layer_->is_visible();
	con_layer_->set_visible(show);
	if (show && con_in_) con_in_->grab_focus();
}

void JVDevTools::_on_console_submit(const String &text) {
	if (!con_out_) return;
	con_in_->clear();
	con_out_->append_text("[color=#ffd75e]> " + text.xml_escape() + "[/color]\n" + run_command(text).xml_escape() + "\n");
}

static String brief(const Variant &v, int max_len = 4000) {
	String s = JSON::stringify(v, " ", true);
	return s.length() > max_len ? s.left(max_len) + "\n..." : s;
}

String JVDevTools::run_command(const String &line) {
	PackedStringArray a = line.strip_edges().split(" ", false);
	if (a.is_empty()) return "";
	const String c = a[0].to_lower();
	if (c == "help") return "status | caps [filter] | mods | libs | log <tag> <0-3|-1> | levels | tree [path] [depth] | node <path> | trace [n] | clear | prof [seconds] | last | worst [n] | hitches | consts | audit | check <NAME> <value> | dump";
	if (c == "status") { Dictionary d = debug_capabilities(); d.erase("capabilities"); d.erase("mods"); d.erase("libraries"); return brief(d); }
	if (c == "caps") {
		Array rows = debug_capabilities().get("capabilities", Array());
		String out;
		for (int i = 0; i < rows.size(); i++) {
			Dictionary r = rows[i];
			if (a.size() > 1 && String(r["id"]).find(a[1]) < 0) continue;
			out += String(r["id"]) + "  " + String(r["status"]) + "  " + String(r["access"]) + (bool(r["open"]) ? "  open" : "  closed") + "\n";
		}
		return out;
	}
	if (c == "mods") return brief(debug_capabilities().get("mods", Dictionary()));
	if (c == "libs") return brief(debug_capabilities().get("libraries", Dictionary()));
	if (c == "log" && a.size() >= 3) return debug_set_log_level(a[1], a[2].to_int()) ? "ok" : "bad tag";
	if (c == "levels") return brief(debug_log_levels());
	if (c == "tree") return brief(debug_tree(a.size() > 1 ? a[1] : String(), a.size() > 2 ? a[2].to_int() : 1));
	if (c == "node" && a.size() > 1) return brief(debug_node(a[1]));
	if (c == "trace") return brief(debug_trace(a.size() > 1 ? a[1].to_int() : 40));
	if (c == "clear") { if (con_out_) con_out_->clear(); return ""; }
	if (c == "prof") return brief(profiler_toggle(a.size() > 1 ? a[1].to_float() : 0.0, "console"));
	if (c == "last") return brief(profiler_summary());
	if (c == "worst") return brief(profiler_worst(a.size() > 1 ? a[1].to_int() : 3));
	if (c == "hitches") return brief(profiler_hitches());
	if (c == "consts") return brief(consts_status());
	if (c == "audit") return brief(consts_audit());
	if (c == "check" && a.size() >= 3) return brief(consts_check_write("console", a[1], a[2].is_valid_int() ? Variant(a[2].to_int()) : Variant(a[2].to_float())));
	if (c == "dump") return debug_dump();
	return "unknown command (help)";
}









Dictionary JVDevTools::consts_live() const {
	Dictionary d;
	if (!is_inside_tree()) { d["available"] = false; d["reason"] = "not in tree"; return d; }
	if (JVModAPI *a = api()) {
		if (a->has_method("player_consts_live")) {
			Variant v = a->call("player_consts_live");
			if (v.get_type() == Variant::DICTIONARY) { d = Dictionary(v).duplicate(); d["source"] = "JVModAPI.player_consts_live"; return d; }
		}
	}
	if (Node *n = get_tree()->get_root()->get_node_or_null(NodePath("JVModGate/JVPlayerConsts"))) {
		if (n->has_method("get_live_statics")) {
			Variant v = n->call("get_live_statics");
			if (v.get_type() == Variant::DICTIONARY) { d = Dictionary(v).duplicate(); d["source"] = "JVPlayerConsts.get_live_statics"; return d; }
		}
	}
	d["source"] = "direct read (fallback)";
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (!rl->has_cached(kPlayerScript)) { d["available"] = false; d["reason"] = "player script not loaded yet"; d["statics"] = Dictionary(); d["consts"] = PackedStringArray(); return d; }
	Ref<Script> s = rl->load(kPlayerScript);
	if (s.is_null()) { d["available"] = false; d["reason"] = "player script missing"; return d; }
	Dictionary cm = s->get_script_constant_map();
	Dictionary st;
	PackedStringArray still;
	int gp_static = 0;
	auto look = [&](const std::string &name, bool gameplay) {
		String n = G(name);
		if (cm.has(n)) {
			still.push_back(n);
			return;
		}
		Variant v = s->get(n);
		if (v.get_type() == Variant::NIL) return;
		Dictionary e;
		e["value"] = v;
		st[n] = e;
		if (gameplay) gp_static++;
	};
	for (auto &n : C::gameplay_names()) look(n, true);
	for (auto &n : C::protocol_names()) look(n, false);
	d["statics"] = st;
	d["consts"] = still;
	const int total = int(C::gameplay_names().size());
	d["available"] = gp_static == total;
	d["reason"] = gp_static == total ? String() : gp_static == 0 ? String("bypass not mounted (player script has the consts)") : "bypass partial: " + String::num_int64(gp_static) + "/" + String::num_int64(total) + " static";
	return d;
}

C::Live JVDevTools::consts_live_struct(const Dictionary &d) const {
	C::Live l;
	l.available = bool(d.get("available", false));
	l.reason = S(String(d.get("reason", "")));
	Dictionary st = d.get("statics", Dictionary());
	Array keys = st.keys();
	for (int i = 0; i < keys.size(); i++) {
		const std::string n = S(String(keys[i]));
		Variant e = st[keys[i]];
		Variant v = e.get_type() == Variant::DICTIONARY ? Dictionary(e).get("value", Variant()) : e;
		C::Spec sp;
		const bool num = v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT;
		l.statics[n] = num ? double(v) : 0.0;
		if (v.get_type() == Variant::INT) sp.is_int = true;
		if (e.get_type() == Variant::DICTIONARY) {
			Dictionary ed = e;
			if (ed.has("stock")) { sp.has_stock = true; sp.stock = double(ed["stock"]); if (Variant(ed["stock"]).get_type() == Variant::INT) sp.is_int = true; }
			if (ed.has("min") && ed.has("max")) { sp.has_bounds = true; sp.min = double(ed["min"]); sp.max = double(ed["max"]); }
			if (ed.has("type")) sp.is_int = String(ed["type"]) == "int";
		}
		l.specs[n] = sp;
	}
	PackedStringArray cs = d.get("consts", PackedStringArray());
	for (int i = 0; i < cs.size(); i++) l.still_const.insert(S(cs[i]));
	return l;
}

C::Context JVDevTools::consts_context() const {
	C::Context c;
	JVModAPI *a = api();
	c.mode = a ? S(a->get_mode()) : "unknown";
	c.cap_open = a ? (a->has_capability("player.consts") ? a->is_open("gameplay") : a->is_open("gameplay")) : false;
	c.synced_lobby = c.mode == "LAN" || c.mode == "private" || c.mode == "friends-only";
	c.is_host = true;
	if (is_inside_tree()) {
		if (Node *net = get_tree()->get_root()->get_node_or_null(NodePath("Net"))) {
			if (bool(net->get("active"))) c.is_host = bool(net->get("is_host"));
		}
	}
	c.match_active = match_active_;
	return c;
}

static Array findings_array(const std::vector<C::Finding> &fs) {
	Array out;
	for (auto &f : fs) {
		Dictionary d;
		d["severity"] = f.sev == C::Sev::Error ? "error" : "warn";
		d["code"] = G(f.code); d["name"] = G(f.name); d["detail"] = G(f.detail); d["source"] = G(f.source);
		out.push_back(d);
	}
	return out;
}

void JVDevTools::note(const std::vector<C::Finding> &fs) {
	const double now = double(Time::get_singleton()->get_ticks_msec());
	for (auto &f : fs) {
		bool found = false;
		for (auto &r : findings_) {
			if (r.f.code == f.code && r.f.name == f.name && r.f.source == f.source) { r.count++; r.last_ms = now; r.f.detail = f.detail; found = true; break; }
		}
		if (!found && findings_.size() < 500) { Rec r; r.f = f; r.first_ms = r.last_ms = now; findings_.push_back(r); }
		String key = G(f.code + "|" + f.name + "|" + f.source);
		if (!warned_.count(key)) {
			warned_[key] = true;
			const String msg = G(f.code) + " " + G(f.name) + " (" + G(f.source) + "): " + G(f.detail);
			if (f.sev == C::Sev::Error) jvm::warn("consts", msg); else jvm::log("consts", msg);
		}
		trace("consts", G(f.code), G(f.name) + " " + G(f.detail));
	}
}

Dictionary JVDevTools::consts_check_write(const String &source, const String &name, const Variant &value) {
	checks_++;
	C::Write w;
	w.name = S(name);
	w.source = S(source);
	w.is_number = value.get_type() == Variant::INT || value.get_type() == Variant::FLOAT;
	w.value = w.is_number ? double(value) : 0.0;
	w.finite = w.is_number && std::isfinite(w.value);
	w.is_integral = value.get_type() == Variant::INT || (w.finite && std::fabs(w.value - std::round(w.value)) < 1e-9);
	C::Live l = consts_live_struct(consts_live());
	std::vector<C::Finding> fs = C::check_write(w, consts_context(), l);
	note(fs);
	const bool ok = !C::has_error(fs);
	if (ok) expect_.approved[w.name] = w.value;
	Dictionary r;
	r["ok"] = ok;
	r["findings"] = findings_array(fs);
	return r;
}

Dictionary JVDevTools::consts_audit() {
	audits_++;
	Dictionary live = consts_live();
	C::Live l = consts_live_struct(live);
	bypass_active_ = false;
	for (auto &n : C::gameplay_names()) if (l.statics.count(n)) { bypass_active_ = true; break; }
	adapter_source_ = live.get("source", "");


	for (auto &[n, v] : l.statics) {
		auto sp = l.specs.find(n);
		if (!expect_.approved.count(n) && (sp == l.specs.end() || !sp->second.has_stock)) expect_.approved[n] = v;
	}
	std::vector<C::Finding> fs = C::audit(l, consts_context(), expect_);
	note(fs);
	Dictionary r;
	r["ok"] = !C::has_error(fs);
	r["findings"] = findings_array(fs);
	r["bypass_active"] = bypass_active_;
	r["source"] = adapter_source_;
	return r;
}

bool JVDevTools::consts_set_host_values(const Dictionary &values) {
	expect_.host_set.clear();
	Array k = values.keys();
	for (int i = 0; i < k.size(); i++) {
		Variant v = values[k[i]];
		if (v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT) expect_.host_set[S(String(k[i]))] = double(v);
	}
	expect_.host_valid = true;
	trace("consts", "host_values", String::num_int64(int64_t(expect_.host_set.size())) + " values");
	if (match_active_) consts_audit();
	return true;
}

void JVDevTools::consts_tick(double dt) {
	if (role_ != Head || !is_inside_tree() || !consts_audit_) return;

	bool m = false;
	Node *root = get_tree()->get_root();
	Node *net = root->get_node_or_null(NodePath("Net"));
	if (net && bool(net->get("active")) && bool(net->get("in_match"))) m = true;
	if (!m) {
		Node *sc = get_tree()->get_current_scene();
		if (sc && sc->get_scene_file_path().ends_with("main.tscn") && sc->get("_rally").get_type() == Variant::OBJECT) m = true;
	}
	if (m != match_active_) {
		match_active_ = m;
		if (m) {
			C::Live l = consts_live_struct(consts_live());
			expect_.start_set.clear();
			for (auto &n : C::gameplay_names()) { auto it = l.statics.find(n); if (it != l.statics.end()) expect_.start_set[n] = it->second; }
			expect_.start_valid = true;
			C::Context c = consts_context();
			if (c.is_host || !c.synced_lobby) { expect_.host_set = expect_.start_set; expect_.host_valid = !expect_.start_set.empty(); }
			trace("consts", "match_start", String::num_int64(int64_t(expect_.start_set.size())) + " values frozen");
			if (!expect_.start_set.empty()) jvm::log("consts", "match start: " + String::num_int64(int64_t(expect_.start_set.size())) + " player consts frozen until match end");
			consts_audit();
		} else {
			expect_.start_valid = false;
			expect_.start_set.clear();
			expect_.host_valid = false;
			expect_.host_set.clear();
			trace("consts", "match_end", "");
		}
	}
	consts_t_ += dt;
	if (consts_t_ < 1.0) return;
	consts_t_ = 0;

	if (!bypass_active_) {
		consts_inactive_t_ += 1.0;
		if (consts_inactive_t_ < 10.0 && audits_ > 0) return;
		consts_inactive_t_ = 0;
	}
	consts_audit();
}

Dictionary JVDevTools::consts_status() const {
	Dictionary d;
	Dictionary live = consts_live();
	d["available"] = live.get("available", false);
	d["reason"] = live.get("reason", "");
	d["source"] = live.get("source", "");
	d["statics"] = int64_t(Dictionary(live.get("statics", Dictionary())).size());
	d["still_const"] = int64_t(PackedStringArray(live.get("consts", PackedStringArray())).size());
	d["bypass_active"] = bypass_active_;
	d["match_active"] = match_active_;
	d["frozen"] = match_active_;
	C::Context c = consts_context();
	d["mode"] = G(c.mode); d["is_host"] = c.is_host; d["synced_lobby"] = c.synced_lobby; d["cap_open"] = c.cap_open;
	auto todict = [](const std::map<std::string, double> &m) { Dictionary o; for (auto &[k, v] : m) o[G(k)] = v; return o; };
	d["start_set_valid"] = expect_.start_valid; d["start_set"] = todict(expect_.start_set);
	d["host_set_valid"] = expect_.host_valid; d["host_set"] = todict(expect_.host_set);
	d["approved"] = todict(expect_.approved);
	int64_t errors = 0;
	for (auto &r : findings_) if (r.f.sev == C::Sev::Error) errors++;
	d["findings"] = int64_t(findings_.size());
	d["errors"] = errors;
	d["audits"] = audits_;
	d["checks"] = checks_;
	d["audit_enabled"] = consts_audit_;
	return d;
}

Array JVDevTools::consts_findings() const {
	Array out;
	for (auto &r : findings_) {
		Dictionary d;
		d["severity"] = r.f.sev == C::Sev::Error ? "error" : "warn";
		d["code"] = G(r.f.code); d["name"] = G(r.f.name); d["detail"] = G(r.f.detail); d["source"] = G(r.f.source);
		d["count"] = r.count; d["first_ms"] = r.first_ms; d["last_ms"] = r.last_ms;
		out.push_back(d);
	}
	return out;
}
