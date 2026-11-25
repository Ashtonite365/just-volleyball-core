


#include "menu_ui.hpp"
#include "../common/api_client.hpp"
jvm::GameIO &jvm_core_ui_io();
#include "picker_ui.hpp"
#include "../common/jvm_common.hpp"
#include "mod_gate.hpp"
#include "devtools.hpp"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/text_edit.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <cmath>

using namespace godot;

static Object *obj_of(const Variant &v) { return v.get_type() == Variant::OBJECT ? v.operator Object *() : nullptr; }

namespace {
struct ModInfo {
	const char *id, *title, *lib, *node, *what, *where;
	std::vector<const char *> files;
	bool full_tier;
};
const std::vector<ModInfo> &mods() {
	static const std::vector<ModInfo> m = {
		{ "textures", "map loader: resource packs + courts", "textures", "/root/JVTextureLoader",
			"Resource packs from mods\\resources\\packs (one folder per pack, found at runtime): outfits, ball, net, court,\n"
			"hats, faces and back accessories. Pick them in the locker accessories section (head / face / extra), or in resources\\packs\\equipped.json.",
			"main menu / offline / LAN / private / friends lobbies: on (synced to players who have the same pack).\n"
			"public lobbies / Quick Match: local only - only you see it, nothing is sent.",
			{ "texture_sync.json", "resources/packs/equipped.json" }, false },
		{ "models", "map loader: player models", "models", "/root/JVModelLoader",
			"Full player models from mods\\models (one folder per model, found at runtime), worn by you; pick one in the\n"
			"locker's full model tab or \"model\": \"<id>\" in models\\equipped.json. The game's animations and hands drive it.",
			"main menu / offline / LAN / private / friends / public lobbies: local only - only you see it, nothing is sent.",
			{ "models/equipped.json" }, false },
		{ "shots", "custom shots", "shots", "/root/JVExtraShots",
			"Air controls on the game's own keys (rally touch IN THE AIR): forward = topspin (exact landing marker),\n"
			"back = Float (the game's own float + its float box), left/right = Curveball (hold longer = further;\n"
			"a fading range box, never the exact spot). Plus bend, sprint+strafe high/low shots and sprint serves.",
			"main menu / offline / LAN / private / friends lobbies: on.\npublic lobbies / Quick Match: blocked.",
			{ "shots/" }, true },
		{ "goofy", "goofy library", "goofy", "/root/JVGoofyMods",
			"Big ball, ball gravity/bounce, player size, max hits per team, no double-hit foul, player consts\n"
			"(player_consts: lobby only; needs player.consts on every player, else the match plays stock).",
			"main menu / offline / LAN / private / friends lobbies: on (host's values apply).\npublic lobbies / Quick Match: blocked.",
			{ "goofy/" }, true },
	};
	return m;
}
static const char *kPlayerConstsTip =
		"Needs player.consts on every player. If anyone in the match doesn't have it, everyone plays the stock values\n"
		"(the host is told who). Edit in the lobby only - locked during a match.";

Array enum_for(const String &key) {
	Array a;
	if (key == "backend") { a.push_back("jv_ball"); a.push_back("rigid"); }
	return a;
}

Variant tidy(const Variant &v) {
	if (v.get_type() == Variant::FLOAT) {
		double d = v;
		if (std::floor(d) == d && std::fabs(d) < 1e15) return int64_t(d);
		double r = std::round(d * 1e6) / 1e6;
		if (std::floor(r) == r) return int64_t(r);
		return r;
	}
	if (v.get_type() == Variant::DICTIONARY) {
		Dictionary in = v, out;
		Array k = in.keys();
		for (int i = 0; i < k.size(); i++) out[k[i]] = tidy(in[k[i]]);
		return out;
	}
	if (v.get_type() == Variant::ARRAY) {
		Array in = v, out;
		for (int i = 0; i < in.size(); i++) out.push_back(tidy(in[i]));
		return out;
	}
	return v;
}
}

PackedStringArray JVMenuUI::config_files(const String &mod_id) {
	PackedStringArray out;
	for (auto &m : mods()) {
		if (mod_id != m.id) continue;
		for (const char *f : m.files) {
			String rel = f;
			if (rel.ends_with("/")) {
				PackedStringArray j = jvm::list_json(jvm::mods_dir(rel.trim_suffix("/")));
				for (int i = 0; i < j.size(); i++) out.push_back(j[i]);
			} else if (FileAccess::file_exists(jvm::mods_root().path_join(rel))) out.push_back(jvm::mods_root().path_join(rel));
		}
	}
	return out;
}

String JVMenuUI::validate_and_save(const String &path, const String &text, bool live) {

	Ref<JSON> j; j.instantiate();
	if (j->parse(text) != OK) return "not saved: invalid JSON at line " + String::num_int64(j->get_error_line()) + ": " + j->get_error_message();
	if (j->get_data().get_type() != Variant::DICTIONARY) return "not saved: the file must be a JSON object { ... }";
	String out = JSON::stringify(tidy(j->get_data()), "  ", false) + "\n";

	String tmp = path + String(".tmp"); String bak = path + String(".bak");
	{
		Ref<FileAccess> f = FileAccess::open(tmp, FileAccess::WRITE);
		if (f.is_null()) return "not saved: cannot write " + tmp;
		f->store_string(out);
		f->close();
	}
	if (JSON::parse_string(FileAccess::get_file_as_string(tmp)).get_type() != Variant::DICTIONARY) {
		DirAccess::remove_absolute(tmp);
		return "not saved: read-back check failed";
	}
	if (FileAccess::file_exists(path)) {
		if (FileAccess::file_exists(bak)) DirAccess::remove_absolute(bak);
		if (DirAccess::copy_absolute(path, bak) != OK) { DirAccess::remove_absolute(tmp); return "not saved: could not back up to " + bak.get_file(); }
	}
	if (DirAccess::rename_absolute(tmp, path) != OK) {

		DirAccess::remove_absolute(path);
		if (DirAccess::rename_absolute(tmp, path) != OK) return "not saved: rename failed (old file kept as " + bak.get_file() + ")";
	}
	jvm::log("menu", "config saved: " + path + " (backup " + bak.get_file() + ")");
	saves_++;
	if (!live) return "saved " + path.get_file() + " (backup: " + bak.get_file() + ")";
	return "saved " + path.get_file() + " (backup: " + bak.get_file() + ")";
}

void JVMenuUI::open_mods_panel() {
	if (panel_id_ && ObjectDB::get_instance(panel_id_)) return;
	Node *menu = Object::cast_to<Node>(ObjectDB::get_instance(menu_id_));
	Node *ui = menu ? Object::cast_to<Node>(obj_of(jvm_core_ui_io().get("ui.menu", menu, "ui"))) : nullptr;
	if (!ui) return;
	Control *root = memnew(Control);
	root->set_name("JvmModsPanel");
	root->set_anchors_preset(Control::PRESET_FULL_RECT);
	root->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	root->set_z_index(50);
	ColorRect *dim = memnew(ColorRect);
	dim->set_name("Backdrop");
	dim->set_color(Color(0, 0, 0, 0.45));
	dim->set_anchors_preset(Control::PRESET_FULL_RECT);
	dim->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	root->add_child(dim);
	PanelContainer *pc = memnew(PanelContainer);
	pc->set_name("Card");
	pc->set_anchors_preset(Control::PRESET_CENTER);
	pc->set_custom_minimum_size(Vector2(1100, 600));
	pc->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
	pc->set_v_grow_direction(Control::GROW_DIRECTION_BOTH);
	Ref<StyleBox> box;
	Variant tv = jvm_core_ui_io().get("ui.menu", menu, "_tilt");
	Button *mods_btn = Object::cast_to<Button>(ObjectDB::get_instance(mods_btn_id_));
	if (tv.get_type() == Variant::ARRAY) {
		Array tilt = tv;
		for (int i = 0; i < tilt.size() && box.is_null(); i++)
			if (Button *b = Object::cast_to<Button>(obj_of(tilt[i]))) if (b != mods_btn && b->has_theme_stylebox_override("normal")) box = b->get_theme_stylebox("normal");
	}
	if (box.is_valid()) {
		Ref<StyleBox> dup = box->duplicate();
		dup->set_content_margin_all(22);
		pc->add_theme_stylebox_override("panel", dup);
	}
	root->add_child(pc);



	VBoxContainer *root_col = memnew(VBoxContainer);
	root_col->set_name("Root");
	root_col->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root_col->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root_col->add_theme_constant_override("separation", 10);
	pc->add_child(root_col);
	HBoxContainer *main = memnew(HBoxContainer);
	main->set_name("Main");
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main->add_theme_constant_override("separation", 16);
	root_col->add_child(main);
	VBoxContainer *side = memnew(VBoxContainer);
	side->set_name("Sidebar");
	side->set_custom_minimum_size(Vector2(300, 0));
	side->set_h_size_flags(Control::SIZE_FILL);
	side->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	side->add_theme_constant_override("separation", 6);
	main->add_child(side);
	Label *sh = memnew(Label);
	sh->set_name("SidebarTitle");
	sh->set_text("mods");
	sh->add_theme_font_size_override("font_size", 18);
	side->add_child(sh);
	ScrollContainer *ss = memnew(ScrollContainer);
	ss->set_name("SidebarScroll");
	ss->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	ss->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	ss->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	side->add_child(ss);
	VBoxContainer *sl = memnew(VBoxContainer);
	sl->set_name("SidebarList");
	sl->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	sl->add_theme_constant_override("separation", 4);
	ss->add_child(sl);
	ColorRect *sep = memnew(ColorRect);
	sep->set_name("Split");
	sep->set_custom_minimum_size(Vector2(1, 0));
	sep->set_color(Color(1, 1, 1, 0.18));
	sep->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	main->add_child(sep);
	VBoxContainer *v = memnew(VBoxContainer);
	v->set_name("View");
	v->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	v->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	v->add_theme_constant_override("separation", 10);
	main->add_child(v);
	ui->add_child(root);
	panel_id_ = root->get_instance_id();
	view_id_ = v->get_instance_id();



	if (Object::cast_to<Object>(obj_of(jvm_core_ui_io().get("ui.menu", menu, "_guide_page"))) == nullptr) { jvm_core_ui_io().set("ui.menu", menu, "_guide_page", root); modal_slot_ = true; }
	if (menu->has_method("_flatten_cards")) jvm_core_ui_io().call("ui.menu", menu, "_flatten_cards");
	root->get_viewport()->gui_release_focus();
	show_list();
	jvm::log("menu", "mods panel opened");
}

void JVMenuUI::clear_view() {
	VBoxContainer *v = Object::cast_to<VBoxContainer>(ObjectDB::get_instance(view_id_));
	if (!v) return;
	for (int i = v->get_child_count() - 1; i >= 0; i--) { Node *c = v->get_child(i); v->remove_child(c); c->queue_free(); }
}

Label *JVMenuUI::add_label(Control *parent, const String &text, int size, const String &name) {
	Label *l = memnew(Label);
	l->set_text(text);
	if (size > 0) l->add_theme_font_size_override("font_size", size);
	l->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	l->set_custom_minimum_size(Vector2(640, 0));
	if (!name.is_empty()) l->set_name(name);
	parent->add_child(l);
	return l;
}

Button *JVMenuUI::add_button(Control *parent, const String &text, const Callable &cb, const String &name) {
	Button *b = memnew(Button);
	b->set_text(text);
	b->set_custom_minimum_size(Vector2(0, 42));
	b->set_focus_mode(Control::FOCUS_ALL);
	b->connect("pressed", cb);
	if (!name.is_empty()) b->set_name(name);
	parent->add_child(b);
	return b;
}

void JVMenuUI::_dev_profiler_toggle() {
	JVDevTools *dt = JVDevTools::get();
	if (!dt) return;
	dt->profiler_toggle(0, "menu");
	if (Button *b = Object::cast_to<Button>(ObjectDB::get_instance(dev_btn_id_))) b->set_text(dt->profiler_menu_label());
}

void JVMenuUI::refresh_sidebar() {
	Control *panel = Object::cast_to<Control>(ObjectDB::get_instance(panel_id_));
	if (!panel) return;
	VBoxContainer *sl = Object::cast_to<VBoxContainer>(panel->find_child("SidebarList", true, false));
	if (!sl) return;
	for (int i = sl->get_child_count() - 1; i >= 0; i--) { Node *c = sl->get_child(i); sl->remove_child(c); c->queue_free(); }
	Dictionary libs = Dictionary(gate_status().get("libraries", Dictionary()));
	Button *first = nullptr;
	for (auto &m : mods()) {
		Dictionary l = libs.get(m.lib, Dictionary());
		String state = l.get("state", "?");
		String on = state == "active" ? "ON" : (state == "?" ? "not installed" : "BLOCKED");
		String label = String(m.title) + "\n" + on;
		Button *b = add_button(sl, label, Callable(this, "show_detail").bind(String(m.id)), String("Mod_") + m.id);
		b->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
		b->set_clip_text(false);
		b->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		b->set_tooltip_text(String(m.title) + "   -   " + on);
		b->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		b->set_custom_minimum_size(Vector2(0, 52));
		b->set_toggle_mode(true);
		b->set_pressed_no_signal(view_ == m.id);
		if (!first) first = b;
	}
	if (view_ == "list" && first) first->call_deferred("grab_focus");
}

void JVMenuUI::show_list() {
	clear_view();
	VBoxContainer *v = Object::cast_to<VBoxContainer>(ObjectDB::get_instance(view_id_));
	if (!v) return;
	view_ = "list";
	refresh_sidebar();
	add_label(v, "jv-mods", 27, "Title");
	Dictionary st = gate_status();
	add_label(v, "mode: " + String(st.get("mode", "?")) + "   (pick a mod for details and settings)", 0, "Mode");
	ScrollContainer *sc = memnew(ScrollContainer);
	sc->set_name("OverviewScroll");
	sc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	sc->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	sc->set_custom_minimum_size(Vector2(0, 200));
	sc->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	v->add_child(sc);
	VBoxContainer *ov = memnew(VBoxContainer);
	ov->set_name("Overview");
	ov->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	sc->add_child(ov);
	Label *body = add_label(ov, panel_text(), 0, "Body");
	body->add_theme_font_size_override("font_size", 14);
	if (JVDevTools *dt = JVDevTools::get()) {
		Button *pb = add_button(v, dt->profiler_menu_label(), Callable(this, "_dev_profiler_toggle"), "JvmDevProfiler");
		pb->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
		dev_btn_id_ = pb->get_instance_id();
	}
	Button *close = add_button(v, "close", Callable(this, "close_mods_panel"), "JvmModsClose");
	close->set_custom_minimum_size(Vector2(200, 46));
}

void JVMenuUI::show_detail(const String &mod_id) {
	const ModInfo *mi = nullptr;
	for (auto &m : mods()) if (mod_id == m.id) mi = &m;
	if (!mi) return;
	clear_view();
	VBoxContainer *v = Object::cast_to<VBoxContainer>(ObjectDB::get_instance(view_id_));
	if (!v) return;
	view_ = mod_id;
	refresh_sidebar();
	add_label(v, mi->title, 27, "Title");
	add_label(v, mi->what, 0, "What");
	add_label(v, String("where it is allowed:\n") + mi->where, 0, "Where");
	Dictionary libs = Dictionary(gate_status().get("libraries", Dictionary()));
	Dictionary l = libs.get(mi->lib, Dictionary());
	String state = l.get("state", "not installed");
	add_label(v, "now: " + String(state == "active" ? "ON" : "BLOCKED") + " - " + String(l.get("reason", state)), 0, "State");
	if (mod_id == "goofy") {
		Label *live = add_label(v, "Ball, player size, gravity, bounce, game speed and rules apply live as you change them (also under System Settings > mods). Overrides still need Save.", 0, "LiveHint");
		live->add_theme_font_size_override("font_size", 14);
	}

	PackedStringArray files = config_files(mod_id);
	bool can_edit = true;
	String lock;
	if (mi->full_tier) {
		JVModGate *gate = Object::cast_to<JVModGate>(get_parent());
		if (gate && !gate->is_active()) { can_edit = false; lock = "settings locked here: " + gate->get_reason(); }
	}
	if (files.is_empty()) add_label(v, "no settings file", 0, "NoFiles");
	ScrollContainer *sc = memnew(ScrollContainer);
	sc->set_name("Scroll");
	sc->set_custom_minimum_size(Vector2(0, 360));
	sc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	sc->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	sc->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	v->add_child(sc);
	VBoxContainer *ed = memnew(VBoxContainer);
	ed->set_name("Editor");
	ed->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	sc->add_child(ed);
	for (int i = 0; i < files.size(); i++) build_file_editor(ed, files[i], can_edit);
	Label *status = add_label(v, lock, 0, "Status");
	status_id_ = status->get_instance_id();
	HBoxContainer *row = memnew(HBoxContainer);
	row->add_theme_constant_override("separation", 10);
	v->add_child(row);
	Button *save = add_button(row, "save", Callable(this, "save_detail"), "JvmSave");
	save->set_disabled(!can_edit || files.is_empty());
	add_button(row, "back", Callable(this, "show_list"), "JvmBack");
	add_button(row, "close", Callable(this, "close_mods_panel"), "JvmModsClose");
	Control *f = nullptr;
	for (int i = 0; i < ed->get_child_count() && !f; i++) f = find_focusable(ed->get_child(i));
	(f ? f : Object::cast_to<Control>(row->get_child(1)))->call_deferred("grab_focus");
}

Control *JVMenuUI::find_focusable(Node *n) {
	Control *c = Object::cast_to<Control>(n);
	if (c && c->get_focus_mode() == Control::FOCUS_ALL && c->is_visible()) return c;
	for (int i = 0; i < n->get_child_count(); i++) if (Control *r = find_focusable(n->get_child(i))) return r;
	return nullptr;
}





static String goofy_live_key(const PackedStringArray &kp) {
	String s;
	for (int i = 0; i < kp.size(); i++) { if (i) s += "."; s += kp[i]; }
	return s;
}
static const char *goofy_live_id(const PackedStringArray &kp) {
	const String key = goofy_live_key(kp);
	if (key == "ball_physics.gravity_scale") return "goofy.gravity_scale";
	if (key == "ball_physics.bounce_scale") return "goofy.bounce_scale";
	if (key == "ball_params.flight.air_drag" || key == "flight.air_drag") return "goofy.air_drag";
	if (key == "ball_params.flight.curve_scale" || key == "flight.curve_scale") return "goofy.curve_scale";
	if (key == "big_ball") return "goofy.ball_size";
	if (key == "player_scale") return "goofy.player_scale";
	if (key == "time_scale") return "goofy.time_scale";
	if (key == "max_hits_per_team") return "goofy.rules.max_team_touches";
	if (key == "no_double_hit") return "goofy.rules.double_hit_foul";
	return nullptr;
}
static Variant goofy_live_shown(Object *api, const char *id, const Variant &file_val) {
	if (!api || !api->has_method("gameplay_info")) return file_val;
	Dictionary info = api->call("gameplay_info", "goofy", String(id));
	Variant local = info.get("local", Variant());
	if (local.get_type() == Variant::NIL) return file_val;
	if (String(id) == "goofy.rules.double_hit_foul") return !bool(local);
	if (String(id) == "goofy.rules.max_team_touches") {
		int t = int(double(local));
		return int64_t(t == 3 ? 0 : t);
	}
	return local;
}

void JVMenuUI::_on_goofy_live(const Variant &value, PackedStringArray keypath) {
	const char *id = goofy_live_id(keypath);
	if (!id) return;
	Object *a = jvm::find_api(this);
	if (!a || !a->has_method("gameplay_set")) return;
	Variant v = value;
	if (String(id) == "goofy.rules.double_hit_foul") v = !bool(value);
	else if (String(id) == "goofy.rules.max_team_touches") v = int(double(value)) <= 0 ? 3 : int(double(value));
	Dictionary r = a->call("gameplay_set", "goofy", String(id), v);
	Label *status = Object::cast_to<Label>(ObjectDB::get_instance(status_id_));
	if (status) {
		if (bool(r.get("ok", false))) status->set_text("applied live (" + goofy_live_key(keypath) + ")");
		else status->set_text("not applied: " + String(r.get("error", "unknown")));
	}
}

void JVMenuUI::add_field(Control *parent, const String &file, const PackedStringArray &keypath, const Variant &val, bool can_edit, int depth) {
	String key = keypath[keypath.size() - 1];
	HBoxContainer *row = memnew(HBoxContainer);
	row->add_theme_constant_override("separation", 10);
	parent->add_child(row);
	Label *l = memnew(Label);
	l->set_text(String("  ").repeat(depth) + key);
	l->set_custom_minimum_size(Vector2(280, 0));
	row->add_child(l);
	Control *w = nullptr;
	Array en = enum_for(key);
	switch (val.get_type()) {
		case Variant::BOOL: {
			CheckButton *c = memnew(CheckButton);
			c->set_pressed(bool(val));
			w = c;
		} break;
		case Variant::INT:
		case Variant::FLOAT: {
			SpinBox *s = memnew(SpinBox);
			double d = val;
			bool whole = std::floor(d) == d;
			s->set_min(-100000); s->set_max(100000);
			s->set_step(whole && (key.contains("max_hits") || key.ends_with("_count")) ? 1.0 : 0.01);
			s->set_value(d);
			s->set_custom_minimum_size(Vector2(160, 0));
			w = s;
		} break;
		case Variant::STRING: {
			if (en.size()) {
				OptionButton *o = memnew(OptionButton);
				for (int i = 0; i < en.size(); i++) { o->add_item(en[i]); if (String(en[i]) == String(val)) o->select(i); }
				w = o;
			} else {
				LineEdit *e = memnew(LineEdit);
				e->set_text(val);
				e->set_custom_minimum_size(Vector2(300, 0));
				w = e;
			}
		} break;
		default: {
			TextEdit *t = memnew(TextEdit);
			t->set_text(JSON::stringify(tidy(val), "  ", false));
			t->set_custom_minimum_size(Vector2(340, 110));
			w = t;
		} break;
	}
	w->set_meta("jvm_file", file);
	w->set_meta("jvm_key", keypath);
	w->set_meta("jvm_type", int(val.get_type()));
	w->set_focus_mode(Control::FOCUS_ALL);
	if (!can_edit) {
		if (BaseButton *b = Object::cast_to<BaseButton>(w)) b->set_disabled(true);
		if (SpinBox *s = Object::cast_to<SpinBox>(w)) s->set_editable(false);
		if (LineEdit *e = Object::cast_to<LineEdit>(w)) e->set_editable(false);
		if (TextEdit *t = Object::cast_to<TextEdit>(w)) t->set_editable(false);
	} else if (view_ == "goofy") {
		if (const char *id = goofy_live_id(keypath)) {
			Variant shown = goofy_live_shown(jvm::find_api(this), id, val);
			if (SpinBox *s = Object::cast_to<SpinBox>(w)) s->set_value(double(shown));
			else if (CheckButton *c = Object::cast_to<CheckButton>(w)) c->set_pressed(bool(shown));
			if (SpinBox *s = Object::cast_to<SpinBox>(w)) s->connect("value_changed", Callable(this, "_on_goofy_live").bind(keypath));
			else if (CheckButton *c = Object::cast_to<CheckButton>(w)) c->connect("toggled", Callable(this, "_on_goofy_live").bind(keypath));
		}
	}
	row->add_child(w);
}

void JVMenuUI::build_file_editor(Control *ed, const String &file, bool can_edit) {
	Label *h = memnew(Label);
	h->set_text(file.get_file() + "   (" + file.get_base_dir().get_file() + ")");
	h->add_theme_font_size_override("font_size", 20);
	ed->add_child(h);
	String text = FileAccess::get_file_as_string(file);
	Variant v = JSON::parse_string(text);
	if (v.get_type() != Variant::DICTIONARY) {
		add_label(ed, "this file is not valid JSON - edit it as text:", 0, "");
	} else {
		Dictionary d = v;
		Array k = d.keys();
		for (int i = 0; i < k.size(); i++) {
			String key = k[i];
			if (key.begins_with("_")) continue;
			Variant val = d[key];
			PackedStringArray path; path.push_back(key);
			if (val.get_type() == Variant::DICTIONARY) {
				Dictionary sub = val;
				Array sk = sub.keys();
				bool flat = true;
				for (int j = 0; j < sk.size(); j++) { int t = Variant(sub[sk[j]]).get_type(); if (t == Variant::DICTIONARY || t == Variant::ARRAY) flat = false; }
				if (flat && sk.size()) {

					const bool pc = key == "player_consts";
					Node *goofy = pc && is_inside_tree() ? get_tree()->get_root()->get_node_or_null(NodePath("JVGoofyMods")) : nullptr;
					const bool pc_locked = goofy && goofy->has_method("player_consts_locked") && bool(goofy->call("player_consts_locked"));
					Label *g = memnew(Label);
					g->set_text(pc ? key + (pc_locked ? String("   (locked during the match)") : String("   (needs player.consts on every player)")) : key);
					if (pc) g->set_tooltip_text(kPlayerConstsTip);
					if (pc) g->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					ed->add_child(g);
					const int first = ed->get_child_count();
					for (int j = 0; j < sk.size(); j++) { PackedStringArray p2 = path; p2.push_back(sk[j]); add_field(ed, file, p2, sub[sk[j]], can_edit && !pc_locked, 1); }
					if (pc) for (int j = first; j < ed->get_child_count(); j++) if (Control *c = Object::cast_to<Control>(ed->get_child(j))) { c->set_tooltip_text(kPlayerConstsTip); c->set_meta("jvm_player_consts", true); }
					continue;
				}
			}
			add_field(ed, file, path, val, can_edit, 0);
		}
	}

	TextEdit *raw = memnew(TextEdit);
	raw->set_name("Raw");
	raw->set_text(text);
	raw->set_meta("jvm_file", file);
	raw->set_meta("jvm_raw_orig", text);
	raw->set_custom_minimum_size(Vector2(640, 140));
	raw->set_visible(v.get_type() != Variant::DICTIONARY);
	raw->set_editable(can_edit);
	raw->set_focus_mode(Control::FOCUS_ALL);
	Button *tog = memnew(Button);
	tog->set_text(v.get_type() != Variant::DICTIONARY ? "raw JSON (shown)" : "edit as raw JSON");
	tog->set_focus_mode(Control::FOCUS_ALL);
	tog->connect("pressed", Callable(this, "_toggle_raw").bind(raw->get_instance_id()));
	ed->add_child(tog);
	ed->add_child(raw);
}

void JVMenuUI::_toggle_raw(uint64_t id) {
	if (TextEdit *t = Object::cast_to<TextEdit>(ObjectDB::get_instance(id))) t->set_visible(!t->is_visible());
}

static void collect(Node *n, std::vector<Control *> &out) {
	if (Control *c = Object::cast_to<Control>(n)) if (c->has_meta("jvm_file")) out.push_back(c);
	for (int i = 0; i < n->get_child_count(); i++) collect(n->get_child(i), out);
}

String JVMenuUI::save_detail() {
	VBoxContainer *v = Object::cast_to<VBoxContainer>(ObjectDB::get_instance(view_id_));
	Label *status = Object::cast_to<Label>(ObjectDB::get_instance(status_id_));
	if (!v) return "";
	std::vector<Control *> ws;
	collect(v, ws);
	Dictionary data;
	Dictionary raw;
	String err;
	for (Control *w : ws) {
		String file = w->get_meta("jvm_file");
		if (w->has_meta("jvm_raw_orig")) {
			TextEdit *t = Object::cast_to<TextEdit>(w);
			if (t->is_visible() && t->get_text() != String(w->get_meta("jvm_raw_orig"))) raw[file] = t->get_text();
			continue;
		}
		if (!data.has(file)) {
			Variant base = JSON::parse_string(FileAccess::get_file_as_string(file));
			data[file] = base.get_type() == Variant::DICTIONARY ? Dictionary(base).duplicate(true) : Dictionary();
		}
		Dictionary d = data[file];
		PackedStringArray kp = w->get_meta("jvm_key");
		Variant val;
		if (CheckButton *c = Object::cast_to<CheckButton>(w)) val = c->is_pressed();
		else if (SpinBox *s = Object::cast_to<SpinBox>(w)) val = s->get_value();
		else if (OptionButton *o = Object::cast_to<OptionButton>(w)) val = o->get_item_text(o->get_selected());
		else if (LineEdit *e = Object::cast_to<LineEdit>(w)) val = e->get_text();
		else if (TextEdit *t = Object::cast_to<TextEdit>(w)) {
			Ref<JSON> j; j.instantiate();
			if (j->parse(t->get_text()) != OK) { err = String(kp[0]) + ": invalid JSON (line " + String::num_int64(j->get_error_line()) + ": " + j->get_error_message() + ")"; break; }
			val = j->get_data();
			int want = int(w->get_meta("jvm_type"));
			if (val.get_type() != want) { err = String(kp[0]) + ": must stay a " + String(want == Variant::ARRAY ? "list [...]" : "object {...}"); break; }
		}
		if (kp.size() == 1) d[kp[0]] = val;
		else { Dictionary sub = d.get(kp[0], Dictionary()); sub[kp[1]] = val; d[kp[0]] = sub; }
	}
	String msg;
	if (!err.is_empty()) msg = "not saved: " + err;
	else {
		Array files = data.keys();
		Array rk = raw.keys();
		for (int i = 0; i < rk.size(); i++) if (!files.has(rk[i])) files.push_back(rk[i]);


		String node0;
		for (auto &m : mods()) if (view_ == m.id) node0 = m.node;
		Node *lib0 = node0.is_empty() ? nullptr : get_tree()->get_root()->get_node_or_null(NodePath(node0.trim_prefix("/root/")));
		for (int i = 0; i < files.size() && msg.is_empty(); i++) {
			String f = files[i];
			String text = raw.has(f) ? String(raw[f]) : JSON::stringify(data[f], "  ", false);
			Variant parsed = JSON::parse_string(text);
			if (parsed.get_type() != Variant::DICTIONARY || !lib0 || !lib0->has_method("validate_config")) continue;
			String why = lib0->call("validate_config", parsed);
			if (!why.is_empty()) msg = "not saved: " + f.get_file() + ": " + why;
		}
		for (int i = 0; i < files.size() && msg.find("not saved") < 0; i++) {
			String f = files[i];
			String text = raw.has(f) ? String(raw[f]) : JSON::stringify(data[f], "  ", false);
			String r = validate_and_save(f, text, true);
			msg += (msg.is_empty() ? "" : "\n") + r;
		}
		if (msg.find("not saved") < 0) {

			String node;
			for (auto &m : mods()) if (view_ == m.id) node = m.node;
			Node *lib = node.is_empty() ? nullptr : get_tree()->get_root()->get_node_or_null(NodePath(node.trim_prefix("/root/")));
			bool live = lib && lib->has_method("reload_config") && bool(lib->call("reload_config"));
			msg += live ? "\napplied now" : "\napplies when this mod is allowed (it is blocked right now)";
			if (view_ == "textures") msg += " (other players see texture changes after the next sync)";
		}
	}
	if (status) status->set_text(msg);
	last_save_msg_ = msg;
	return msg;
}

void JVMenuUI::close_mods_panel() {
	Node *menu = Object::cast_to<Node>(ObjectDB::get_instance(menu_id_));
	Object *p = ObjectDB::get_instance(panel_id_);
	if (menu && modal_slot_ && obj_of(jvm_core_ui_io().get("ui.menu", menu, "_guide_page")) == p) jvm_core_ui_io().set("ui.menu", menu, "_guide_page", Variant());
	modal_slot_ = false;
	if (Node *n = Object::cast_to<Node>(p)) n->queue_free();
	panel_id_ = 0; view_id_ = 0; view_ = "";
	if (menu && menu->has_method("_modal_closed")) jvm_core_ui_io().call("ui.menu", menu, "_modal_closed");
	if (Button *b = Object::cast_to<Button>(ObjectDB::get_instance(mods_btn_id_))) if (b->is_inside_tree()) b->grab_focus();
}


void JVMenuUI::guard_frame() {
	Node *menu = Object::cast_to<Node>(ObjectDB::get_instance(menu_id_));
	if (!menu || !menu->is_inside_tree()) return;
	Control *panel = Object::cast_to<Control>(ObjectDB::get_instance(panel_id_));
	if (panel) {
		Input *in = Input::get_singleton();
		if (in->is_action_just_pressed("ui_cancel")) {
			if (view_ != "list") show_list(); else { close_mods_panel(); return; }
		}

		Control *f = panel->get_viewport()->gui_get_focus_owner();
		if (!f || !panel->is_ancestor_of(f)) {
			if (Control *c = find_focusable(panel)) c->grab_focus();
		}
		if (modal_slot_ && obj_of(jvm_core_ui_io().get("ui.menu", menu, "_guide_page")) == nullptr) jvm_core_ui_io().set("ui.menu", menu, "_guide_page", panel);
	}


	bool open = panel != nullptr;
	if (!open && menu->has_method("_modal_up")) open = bool(jvm_core_ui_io().call("ui.menu", menu, "_modal_up"));
	const char *vis_members[] = { "_friends_panel", "_cpu_editor", "_modifiers", "_pw_panel" };
	for (const char *m : vis_members) {
		if (open) break;
		Control *c = Object::cast_to<Control>(obj_of(jvm_core_ui_io().get("ui.menu", menu, m)));
		if (c && c->is_visible_in_tree()) open = true;
	}
	if (!open) {
		Object *pk = obj_of(jvm_core_ui_io().get("ui.menu", menu, "_fp_picker"));
		if (pk) { Control *r = Object::cast_to<Control>(obj_of(jvm_core_ui_io().get("ui.menu", pk, "_root"))); if (r && r->is_visible_in_tree()) open = true; }
	}
	if (!open) {
		if (JVPickerUI *pu = Object::cast_to<JVPickerUI>(get_node_or_null(NodePath("JVPickerUI"))))
			if (pu->ball_picker_open()) open = true;
	}
	if (!open) {
		for (int i = 0; i < menu->get_child_count() && !open; i++)
			if (Window *w = Object::cast_to<Window>(menu->get_child(i))) if (w->is_visible()) open = true;
	}
	if (!open) {
		if (Object::cast_to<Object>(ui_select_active())) open = true;
	}



	if (open != overlay_open_ || open) suppress_cards(menu, open);
	overlay_open_ = open;
}

Object *JVMenuUI::ui_select_active() {
	if (ui_select_script_.is_null()) return nullptr;
	return obj_of(jvm_core_ui_io().get("ui.style", ui_select_script_, "active"));
}

void JVMenuUI::suppress_cards(Node *menu, bool on) {
	const char *lists[] = { "_tilt", "_lean" };
	for (const char *ln : lists) {
		Variant lv = jvm_core_ui_io().get("ui.menu", menu, ln);
		if (lv.get_type() != Variant::ARRAY) continue;
		Array a = lv;
		for (int i = 0; i < a.size(); i++) {
			Control *c = Object::cast_to<Control>(obj_of(a[i]));
			if (!c) continue;
			if (on) {
				static const StringName kOtEnabled("offset_transform_enabled");
				c->set(kOtEnabled, false);
				c->set_z_index(0);
				if (!c->has_meta("jvm_mf")) { c->set_meta("jvm_mf", int(c->get_mouse_filter())); c->set_mouse_filter(Control::MOUSE_FILTER_IGNORE); }
			} else if (c->has_meta("jvm_mf")) {
				c->set_mouse_filter(Control::MouseFilter(int(c->get_meta("jvm_mf"))));
				c->remove_meta("jvm_mf");
			}
		}
	}
}
