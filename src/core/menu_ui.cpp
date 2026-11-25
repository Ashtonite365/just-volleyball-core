#include "menu_ui.hpp"
#include "../common/jv_prof.hpp"
#include "../common/api_client.hpp"
#include "picker_ui.hpp"
#include "../common/jvm_common.hpp"
#include "mod_gate.hpp"
#include "devtools.hpp"
#include <godot_cpp/classes/button.hpp>

#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <algorithm>
#include <vector>

using namespace godot;




static const char *const kCoreUiCaps[] = { "gate.mode", "config.files", "config.validate", "scene.current", "ui.menu", "ui.style",
	"ui.notify", "ui.mods_panel", "menu.perf", "stage.picker", "stage.pick", "ball.pick", "ball.skin", "packs.query", "packs.rescan",
	"net.party_ball", "lib.status", "lib.reload", "goofy.query", "goofy.ball_mode" };
jvm::GameIO &jvm_core_ui_io() {
	static jvm::GameIO io = [] {
		jvm::GameIO g;
		g.mod = "core_ui";
		g.writes_need_open = false;
		for (const char *c : kCoreUiCaps) g.caps.push_back(c);
		return g;
	}();
	return io;
}

void JVMenuUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_mods_panel"), &JVMenuUI::open_mods_panel);
	ClassDB::bind_method(D_METHOD("close_mods_panel"), &JVMenuUI::close_mods_panel);
	ClassDB::bind_method(D_METHOD("panel_text"), &JVMenuUI::panel_text);
	ClassDB::bind_method(D_METHOD("get_info"), &JVMenuUI::get_info);
	ClassDB::bind_method(D_METHOD("show_list"), &JVMenuUI::show_list);
	ClassDB::bind_method(D_METHOD("show_detail", "mod_id"), &JVMenuUI::show_detail);
	ClassDB::bind_method(D_METHOD("save_detail"), &JVMenuUI::save_detail);
	ClassDB::bind_method(D_METHOD("validate_and_save", "path", "text", "live"), &JVMenuUI::validate_and_save);
	ClassDB::bind_method(D_METHOD("config_files", "mod_id"), &JVMenuUI::config_files);
	ClassDB::bind_method(D_METHOD("_toggle_raw", "id"), &JVMenuUI::_toggle_raw);
	ClassDB::bind_method(D_METHOD("_on_goofy_live", "value", "keypath"), &JVMenuUI::_on_goofy_live);
	ClassDB::bind_method(D_METHOD("_dev_profiler_toggle"), &JVMenuUI::_dev_profiler_toggle);
}

static Object *obj(const Variant &v) { return v.get_type() == Variant::OBJECT ? v.operator Object *() : nullptr; }

static bool has_label_text(Node *n, const String &text, int depth = 0) {
	if (!n || depth > 6) return false;
	if (Label *l = Object::cast_to<Label>(n)) if (l->get_text() == text) return true;
	for (int i = 0; i < n->get_child_count(); i++) if (has_label_text(n->get_child(i), text, depth + 1)) return true;
	return false;
}

Dictionary JVMenuUI::get_info() const {
	Dictionary d;
	d["injections"] = injections_;
	d["menu_injected"] = menu_id_ != 0 && ObjectDB::get_instance(menu_id_) != nullptr;
	d["panel_open"] = panel_id_ != 0 && ObjectDB::get_instance(panel_id_) != nullptr;
	d["last_skip"] = last_skip_;
	d["view"] = view_;
	d["overlay_open"] = overlay_open_;
	d["modal_slot"] = modal_slot_;
	d["saves"] = saves_;
	d["last_save"] = last_save_msg_;
	return d;
}

void JVMenuUI::skip(Node *menu, const String &why) {
	if (skipped_menu_ == menu->get_instance_id() && last_skip_ == why) return;
	skipped_menu_ = menu->get_instance_id();
	last_skip_ = why;
	jvm::log("menu", "menu UI not changed: " + why);
}

void JVMenuUI::_ready() {
	set_process_priority(1000);
	if (!get_node_or_null(NodePath("JVPickerUI"))) {
		JVPickerUI *p = memnew(JVPickerUI);
		p->set_name("JVPickerUI");
		add_child(p);
	}
}

void JVMenuUI::_process(double delta) {
	JVM_PROF("core.menu_ui_process");
	guard_frame();
	t_ += delta;
	if (t_ < 0.25) return;
	t_ = 0;
	if (dev_btn_id_) {
		if (Button *b = Object::cast_to<Button>(ObjectDB::get_instance(dev_btn_id_))) {
			if (b->is_visible_in_tree() && JVDevTools::get()) b->set_text(JVDevTools::get()->profiler_menu_label());
		} else dev_btn_id_ = 0;
	}
	JVModGate *gate = Object::cast_to<JVModGate>(get_parent());
	if (!gate || !gate->pin_ok()) return;
	Node *menu = jvm::current_scene(this);
	if (!menu) return;
	if (menu->get_instance_id() == menu_id_) {

		Control *c = Object::cast_to<Control>(ObjectDB::get_instance(credits_id_));
		Control *d = Object::cast_to<Control>(ObjectDB::get_instance(discord_id_));
		if (c && d && c->is_visible() != d->is_visible()) c->set_visible(d->is_visible());
		return;
	}

	if (!menu->has_method("_card_btn") || !menu->has_method("_open_credits")) return;
	if (menu->has_meta("jvm_menu_ui")) { menu_id_ = menu->get_instance_id(); return; }
	try_inject(menu);
}

bool JVMenuUI::try_inject(Node *menu) {

	Control *home = Object::cast_to<Control>(obj(jvm_core_ui_io().get("ui.menu", menu, "_home")));
	HBoxContainer *bar = Object::cast_to<HBoxContainer>(obj(jvm_core_ui_io().get("ui.menu", menu, "_bottom_bar")));
	Button *discord = Object::cast_to<Button>(obj(jvm_core_ui_io().get("ui.menu", menu, "_discord_btn")));
	Variant tv = jvm_core_ui_io().get("ui.menu", menu, "_tilt");
	Node *ui = Object::cast_to<Node>(obj(jvm_core_ui_io().get("ui.menu", menu, "ui")));
	if (!home) { skip(menu, "home page not built yet"); return false; }
	if (!bar) { skip(menu, "_bottom_bar missing"); return false; }
	if (!discord || discord->get_parent() != bar) { skip(menu, "discord button missing"); return false; }
	if (tv.get_type() != Variant::ARRAY) { skip(menu, "_tilt missing"); return false; }
	if (!ui) { skip(menu, "ui layer missing"); return false; }
	Array tilt = tv;

	const char *titles[] = { "online", "local", "practice", "controller config", "system settings", "feedback & bug reports", "credits" };
	Button *card[7] = {};
	for (int i = 0; i < tilt.size(); i++) {
		Button *b = Object::cast_to<Button>(obj(tilt[i]));
		if (!b) continue;
		for (int k = 0; k < 7; k++) if (!card[k] && has_label_text(b, titles[k])) card[k] = b;
	}
	for (int k = 0; k < 7; k++) if (!card[k]) { skip(menu, String(titles[k]) + " card not found"); return false; }
	Button *credits = card[6];
	Control *wrap = Object::cast_to<Control>(credits->get_parent());
	BoxContainer *list = wrap ? Object::cast_to<BoxContainer>(wrap->get_parent()) : nullptr;
	if (!wrap || !list) { skip(menu, "home card list not found"); return false; }
	for (int k = 0; k < 6; k++) if (!card[k]->get_parent() || card[k]->get_parent()->get_parent() != list) { skip(menu, String(titles[k]) + " card not in the home list"); return false; }

	Ref<Script> ui_style;
	{
		TypedArray<Dictionary> gl = ProjectSettings::get_singleton()->get_global_class_list();
		for (int i = 0; i < gl.size(); i++) {
			Dictionary d = gl[i];
			if (String(d.get("class", "")) == "UiStyle") { ui_style = jvm_core_ui_io().resource("ui.style", String(d.get("path", ""))); break; }
		}
	}
	if (ui_style.is_null()) { skip(menu, "UiStyle not found"); return false; }
	{
		TypedArray<Dictionary> gl = ProjectSettings::get_singleton()->get_global_class_list();
		for (int i = 0; i < gl.size(); i++) { Dictionary d = gl[i]; if (String(d.get("class", "")) == "UiSelect") ui_select_script_ = jvm_core_ui_io().resource("ui.style", String(d.get("path", ""))); }
	}
	Dictionary consts = ui_style->get_script_constant_map();
	if (!consts.has("PRIMARY") || !consts.has("INK")) { skip(menu, "UiStyle colours missing"); return false; }
	if (!menu->has_method("_tint_button")) { skip(menu, "_tint_button missing"); return false; }




	Color disc = consts.has("PRIMARY") ? Color(consts["PRIMARY"]) : Color(1, 0.82, 0.25);
	Variant made = jvm_core_ui_io().call("ui.menu", menu, "_card_btn", "wrench", "mods", "skins, shots, goofy library & their settings", "info", Callable(this, "open_mods_panel"), false, disc);
	Control *mods_wrap = Object::cast_to<Control>(obj(made));
	Button *mods_btn = mods_wrap && mods_wrap->get_child_count() > 0 ? Object::cast_to<Button>(mods_wrap->get_child(0)) : nullptr;
	if (!mods_btn) { if (mods_wrap) mods_wrap->queue_free(); skip(menu, "_card_btn did not return a card"); return false; }
	mods_wrap->set_name("JvmModsCard");
	list->add_child(mods_wrap);




	Control *order[7] = { Object::cast_to<Control>(card[0]->get_parent()), Object::cast_to<Control>(card[1]->get_parent()),
		Object::cast_to<Control>(card[2]->get_parent()), mods_wrap, Object::cast_to<Control>(card[3]->get_parent()),
		Object::cast_to<Control>(card[4]->get_parent()), Object::cast_to<Control>(card[5]->get_parent()) };
	const int base = order[0]->get_index();
	for (int k = 0; k < 7; k++) list->move_child(order[k], base + k);

	tilt.erase(credits);
	{
		Button *want[7] = { card[0], card[1], card[2], mods_btn, card[3], card[4], card[5] };
		std::vector<int> slots;
		for (int i = 0; i < tilt.size(); i++) {
			Button *b = Object::cast_to<Button>(obj(tilt[i]));
			for (Button *w : want) if (b == w) { slots.push_back(i); break; }
		}
		std::sort(slots.begin(), slots.end());
		for (size_t i = 0; i < slots.size() && i < 7; i++) tilt[slots[i]] = want[i];
	}



	list->remove_child(wrap);
	wrap->queue_free();
	Button *cb = Object::cast_to<Button>(discord->duplicate(Node::DUPLICATE_SCRIPTS | Node::DUPLICATE_GROUPS));
	if (!cb) { skip(menu, "could not copy the discord button"); return false; }
	cb->set_name("JvmCreditsButton");
	cb->set_text("credits");
	cb->set_tooltip_text("credits");
	cb->set_focus_neighbor(SIDE_LEFT, NodePath()); cb->set_focus_neighbor(SIDE_RIGHT, NodePath());



	Ref<Texture2D> tex;
	if (ResourceLoader::get_singleton()->exists("res://game/ui_icon/Game Icons/PNG/Black/2x/movie.png"))
		tex = Ref<Texture2D>(Object::cast_to<Texture2D>(obj(jvm_core_ui_io().call("ui.style", ui_style, "icon", "movie", false, true))));
	cb->set_button_icon(tex);
	jvm_core_ui_io().call("ui.menu", menu, "_tint_button", cb, consts["PRIMARY"]);
	Color ink = consts["INK"];
	for (const char *fc : { "font_color", "font_hover_color", "font_pressed_color", "font_focus_color", "font_hover_pressed_color" })
		cb->add_theme_color_override(fc, ink);
	cb->connect("pressed", Callable(menu, "_open_credits"));
	bar->add_child(cb);
	bar->move_child(cb, discord->get_index() + 1);
	cb->set_visible(discord->is_visible());
	credits = cb;


	Button *btns[7] = { card[0], card[1], card[2], mods_btn, card[3], card[4], card[5] };
	for (int i = 0; i < 7; i++) {
		btns[i]->set_focus_neighbor(SIDE_TOP, i > 0 ? btns[i]->get_path_to(btns[i - 1]) : NodePath());
		btns[i]->set_focus_neighbor(SIDE_BOTTOM, i < 6 ? btns[i]->get_path_to(btns[i + 1]) : btns[i]->get_path_to(credits));
	}
	credits->set_focus_neighbor(SIDE_TOP, credits->get_path_to(btns[6]));
	credits->set_focus_neighbor(SIDE_LEFT, credits->get_path_to(discord));

	menu->set_meta("jvm_menu_ui", true);
	menu_id_ = menu->get_instance_id();
	credits_id_ = credits->get_instance_id();
	discord_id_ = discord->get_instance_id();
	mods_btn_id_ = mods_btn->get_instance_id();
	injections_++;
	jvm::log("menu", "menu UI: home order online/local/practice/mods/controller config/system settings/feedback; credits = yellow bottom-bar button right of discord");
	return true;
}

Dictionary JVMenuUI::gate_status() {
	JVModGate *gate = Object::cast_to<JVModGate>(get_parent());
	return gate ? gate->build_status() : Dictionary();
}

String JVMenuUI::panel_text() {
	Dictionary st = gate_status();
	String t = "mode: " + String(st.get("mode", "?")) + "\n\n";
	Dictionary libs = st.get("libraries", Dictionary());
	const char *order[] = { "core", "loader", "textures", "models", "shots", "goofy" };
	for (const char *n : order) {
		if (!libs.has(n)) continue;
		Dictionary l = libs[n];
		String state = l.get("state", "?");
		String on = state == "active" ? "ON" : (state == "loaded" ? "loaded" : "BLOCKED");
		t += String(n) + ": " + on + "  - " + String(l.get("reason", state)) + "\n";
	}
	String rel = DirAccess::dir_exists_absolute(jvm::mods_dir("resources/packs")) || !DirAccess::dir_exists_absolute(jvm::mods_dir("resource packs")) ? String("resources/packs") : String("resource packs");
	t += "\nresource packs (mods\\" + rel.replace("/", "\\") + "):\n";
	String root = jvm::mods_dir(rel);
	PackedStringArray dirs = DirAccess::get_directories_at(root);
	int n = 0;
	Dictionary seen, equipped;
	{
		Variant ev = JSON::parse_string(FileAccess::get_file_as_string(root.path_join("equipped.json")));
		if (ev.get_type() == Variant::DICTIONARY) {
			Array a = Dictionary(ev).get("equipped", Array());
			for (int i = 0; i < a.size(); i++) equipped[String(a[i]).to_upper()] = true;
		}
	}
	dirs.sort();
	for (int i = 0; i < dirs.size(); i++) {
		String f = root.path_join(dirs[i]).path_join("skin.json");
		if (!FileAccess::file_exists(f)) continue;
		Variant v = JSON::parse_string(FileAccess::get_file_as_string(f));
		Dictionary d = v.get_type() == Variant::DICTIONARY ? Dictionary(v) : Dictionary();
		String id = d.get("id", "?");
		String note = seen.has(id.to_upper()) ? "  [duplicate ID, ignored]" : (equipped.has(id.to_upper()) ? "  [equipped]" : "");
		seen[id.to_upper()] = true;
		t += "  " + id + "  " + String(d.get("name", dirs[i])) + "  (" + dirs[i] + ")" + note + "\n";
		n++;
	}
	if (n == 0) t += "  (none installed)\n";
	return t;
}
