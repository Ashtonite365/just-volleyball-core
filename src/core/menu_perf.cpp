#include "menu_perf.hpp"
#include "../common/jv_prof.hpp"
#include "../common/api_client.hpp"
jvm::GameIO &jvm_core_ui_io();
#include "../common/jvm_common.hpp"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/classes/object.hpp>
#include <cmath>

using namespace godot;

static Object *obj(const Variant &v) {
	return v.get_type() == Variant::OBJECT ? v.operator Object *() : nullptr;
}

void JVMenuPerf::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "on"), &JVMenuPerf::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &JVMenuPerf::is_enabled);
	ClassDB::bind_method(D_METHOD("get_info"), &JVMenuPerf::get_info);
	ClassDB::bind_method(D_METHOD("reset_counters"), &JVMenuPerf::reset_counters);
}

void JVMenuPerf::setup(Mode mode, JVMenuPerf *host) {
	mode_ = mode;
	host_ = host;
}

void JVMenuPerf::load_cfg() {
	if (loaded_cfg_) return;
	loaded_cfg_ = true;
	Dictionary cfg = jvm::load_category_config("core");
	if (cfg.has("menu_perf")) enabled_ = bool(cfg["menu_perf"]);
	else enabled_ = true;
}

void JVMenuPerf::_ready() {
	if (mode_ != Host) return;
	load_cfg();
	set_process(false);
	JVMenuPerf *pre = memnew(JVMenuPerf);
	pre->set_name("pre");
	pre->setup(Pre, this);
	pre->set_process_priority(-1000);
	add_child(pre);
	JVMenuPerf *post = memnew(JVMenuPerf);
	post->set_name("post");
	post->setup(Post, this);
	post->set_process_priority(1);
	add_child(post);
	jvm::log("menu", String("menu_perf ") + (enabled_ ? "ON" : "OFF") + " (mods/core menu_perf)");
}

void JVMenuPerf::set_enabled(bool on) {
	if (host_) { host_->set_enabled(on); return; }
	enabled_ = on;
}

void JVMenuPerf::reset_counters() {
	if (host_) { host_->reset_counters(); return; }
	frames_ = tilt_runs_ = z_skipped_ = strip_skipped_ = strip_writes_ = 0;
}

Dictionary JVMenuPerf::get_info() const {
	if (host_) return host_->get_info();
	Dictionary d;
	d["enabled"] = enabled_;
	d["frames"] = frames_;
	d["tilt_runs"] = tilt_runs_;
	d["z_skipped"] = z_skipped_;
	d["strip_skipped"] = strip_skipped_;
	d["strip_writes"] = strip_writes_;
	return d;
}

Node *JVMenuPerf::menu() const {
	JVMenuPerf *h = host_ ? host_ : const_cast<JVMenuPerf *>(this);
	if (h->menu_id_) {
		if (Node *n = Object::cast_to<Node>(ObjectDB::get_instance(h->menu_id_))) {
			if (n->is_inside_tree()) return n;
		}
	}
	if (!get_tree()) return nullptr;
	Node *sc = get_tree()->get_current_scene();
	if (sc && sc->has_method("_tilt_cards") && sc->has_method("_place_strips")) {
		h->menu_id_ = sc->get_instance_id();
		return sc;
	}
	return nullptr;
}

namespace {


struct OtNames { StringName en{ "offset_transform_enabled" }, pivot{ "offset_transform_pivot_ratio" }, rot{ "offset_transform_rotation" }, scale{ "offset_transform_scale" }, pos{ "offset_transform_position" }; };
const OtNames &ot() { static const OtNames n; return n; }
}

void JVMenuPerf::_process(double delta) {
	JVM_PROF("core.menu_perf_process");
	if (mode_ == Pre && host_) host_->pre_tick();
	else if (mode_ == Post && host_) host_->post_tick(delta);
	(void)delta;
}

void JVMenuPerf::pre_tick() {
	if (!enabled_) return;
	Node *m = menu();
	if (!m) return;
	frames_++;

	if (bool(jvm_core_ui_io().get("menu.perf", m, "_freeplay"))) { stole_tilt_ = stole_strips_ = false; return; }
	if (m->has_method("_modal_up") && bool(jvm_core_ui_io().call("menu.perf", m, "_modal_up"))) { stole_tilt_ = stole_strips_ = false; return; }

	saved_tilt_ = jvm_core_ui_io().get("menu.perf", m, "_tilt");
	saved_lean_ = jvm_core_ui_io().get("menu.perf", m, "_lean");
	jvm_core_ui_io().set("menu.perf", m, "_tilt", Array());
	jvm_core_ui_io().set("menu.perf", m, "_lean", Array());
	stole_tilt_ = true;



	Control *top_p = Object::cast_to<Control>(obj(jvm_core_ui_io().get("menu.perf", m, "_top_panel")));
	Control *bot_p = Object::cast_to<Control>(obj(jvm_core_ui_io().get("menu.perf", m, "_bottom_panel")));
	int top_h = top_p ? int(std::lround(top_p->get_size().y)) : -1;
	int bot_h = bot_p ? int(std::lround(bot_p->get_size().y)) : -1;
	if (top_h == last_top_h_ && bot_h == last_bottom_h_ && last_top_h_ >= 0) {
		saved_top_strip_ = jvm_core_ui_io().get("menu.perf", m, "_top_strip");
		saved_bottom_strip_ = jvm_core_ui_io().get("menu.perf", m, "_bottom_strip");
		jvm_core_ui_io().set("menu.perf", m, "_top_strip", Variant());
		jvm_core_ui_io().set("menu.perf", m, "_bottom_strip", Variant());
		stole_strips_ = true;
		strip_skipped_++;
	} else {
		stole_strips_ = false;
		last_top_h_ = top_h;
		last_bottom_h_ = bot_h;
		strip_writes_++;
	}
}

void JVMenuPerf::post_tick(double delta) {
	if (!enabled_) return;
	Node *m = menu();
	if (!m) return;
	if (stole_tilt_) {
		jvm_core_ui_io().set("menu.perf", m, "_tilt", saved_tilt_);
		jvm_core_ui_io().set("menu.perf", m, "_lean", saved_lean_);
		stole_tilt_ = false;
		tilt_cards(m, delta);
		lean_tiles(m, delta);
		tilt_runs_++;
	}
	if (stole_strips_) {
		jvm_core_ui_io().set("menu.perf", m, "_top_strip", saved_top_strip_);
		jvm_core_ui_io().set("menu.perf", m, "_bottom_strip", saved_bottom_strip_);
		stole_strips_ = false;
	}
}


void JVMenuPerf::tilt_cards(Node *menu, double delta) {
	Array tilt = jvm_core_ui_io().get("menu.perf", menu, "_tilt");
	if (bool(jvm_core_ui_io().get("menu.perf", menu, "_freeplay")) || tilt.is_empty()) return;
	Viewport *size_vp = menu->get_viewport();
	if (!size_vp) return;
	Vector2 vp = size_vp->get_visible_rect().size;
	if (vp.x <= 0.0) return;
	Node *ui = Object::cast_to<Node>(obj(jvm_core_ui_io().get("menu.perf", menu, "ui")));
	Viewport *mouse_vp = ui && ui->get_viewport() ? ui->get_viewport() : size_vp;
	Vector2 mouse = mouse_vp->get_mouse_position();
	Vector2 off = (mouse / vp) - Vector2(0.5, 0.5);
	double fake = double(jvm_core_ui_io().get("menu.perf", menu, "_tilt_fake"));
	if (fake != 0.0) off.x = real_t(fake);
	real_t k = Math::clamp(real_t(delta) * 9.0f, 0.0f, 1.0f);
	int depth = 0;
	for (int i = 0; i < tilt.size(); i++) {
		Control *card = Object::cast_to<Control>(obj(tilt[i]));
		if (!card || !card->is_visible_in_tree()) continue;
		card->set(ot().en, true);
		card->set(ot().pivot, Vector2(0.5, 0.5));
		bool hovered = card->has_focus() || card->get_global_rect().has_point(mouse);
		real_t recede = 1.0f - 0.026f * real_t(depth);
		real_t fan = 0.028f - 0.016f * real_t(depth);
		real_t want_rot = hovered ? 0.0f : fan - off.x * 0.06f;
		real_t want_scale = hovered ? 1.07f : recede - Math::abs(off.x) * 0.012f;
		real_t want_x = (hovered ? 52.0f : 8.0f * real_t(depth)) - off.x * (26.0f + 8.0f * real_t(depth));
		real_t rot = real_t(card->get(ot().rot));
		card->set(ot().rot, Math::lerp(rot, want_rot, k));
		Vector2 sc = card->get(ot().scale);
		card->set(ot().scale, sc.lerp(Vector2(want_scale, want_scale), k));
		Vector2 pos = card->get(ot().pos);
		pos.x = Math::lerp(pos.x, want_x, k);
		card->set(ot().pos, pos);
		int want_z = hovered ? 2 : 0;
		if (card->get_z_index() != want_z) card->set_z_index(want_z);
		else z_skipped_++;
		depth++;
	}
}


void JVMenuPerf::lean_tiles(Node *menu, double delta) {
	Array lean = jvm_core_ui_io().get("menu.perf", menu, "_lean");
	if (bool(jvm_core_ui_io().get("menu.perf", menu, "_freeplay")) || lean.is_empty()) return;
	Viewport *size_vp = menu->get_viewport();
	if (!size_vp) return;
	Vector2 vp = size_vp->get_visible_rect().size;
	if (vp.x <= 0.0) return;
	Node *ui = Object::cast_to<Node>(obj(jvm_core_ui_io().get("menu.perf", menu, "ui")));
	Viewport *mouse_vp = ui && ui->get_viewport() ? ui->get_viewport() : size_vp;
	Vector2 mouse = mouse_vp->get_mouse_position();
	Vector2 off = (mouse / vp) - Vector2(0.5, 0.5);
	double fake = double(jvm_core_ui_io().get("menu.perf", menu, "_tilt_fake"));
	if (fake != 0.0) off.x = real_t(fake);
	real_t k = Math::clamp(real_t(delta) * 9.0f, 0.0f, 1.0f);
	for (int i = 0; i < lean.size(); i++) {
		Control *t = Object::cast_to<Control>(obj(lean[i]));
		if (!t || !t->is_visible_in_tree()) continue;
		t->set(ot().en, true);
		t->set(ot().pivot, Vector2(0.5, 0.5));
		bool hovered = t->get_global_rect().has_point(mouse);
		real_t want_rot = hovered ? 0.0f : -off.x * 0.05f;
		real_t want_scale = hovered ? 1.08f : 1.0f - Math::abs(off.x) * 0.01f;
		real_t rot = real_t(t->get(ot().rot));
		t->set(ot().rot, Math::lerp(rot, want_rot, k));
		Vector2 sc = t->get(ot().scale);
		t->set(ot().scale, sc.lerp(Vector2(want_scale, want_scale), k));
		Vector2 pos = t->get(ot().pos);
		pos.x = Math::lerp(pos.x, -off.x * 14.0f, k);
		t->set(ot().pos, pos);
		int want_z = hovered ? 2 : 0;
		if (t->get_z_index() != want_z) t->set_z_index(want_z);
		else z_skipped_++;
	}
}
