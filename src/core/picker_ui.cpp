#include "picker_ui.hpp"
#include "../common/jv_prof.hpp"
#include "../common/api_client.hpp"
jvm::GameIO &jvm_core_ui_io();

#include "../common/jvm_common.hpp"
#include "mod_api.hpp"

#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/window.hpp>

#include <vector>

using namespace godot;

static const Vector2 kTile(200, 86);
static const int kCols = 4;
static const char *kGameBalls[][3] = {

	{ "nationals", "pro", "res://game/images/gameballs/ball_nationals.png" },
	{ "school", "school", "res://game/images/gameballs/ball_school.png" },
	{ "beach", "outdoor", "res://game/images/gameballs/ball_outdoor.png" },
};
static const int kGameBallCount = 3;

void JVPickerUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_ball_mode", "mode"), &JVPickerUI::_on_ball_mode);
	ClassDB::bind_method(D_METHOD("ball_override_visible"), &JVPickerUI::ball_override_visible);
	ClassDB::bind_method(D_METHOD("_on_ball_btn"), &JVPickerUI::_on_ball_btn);
	ClassDB::bind_method(D_METHOD("_on_stage_btn"), &JVPickerUI::_on_stage_btn);
	ClassDB::bind_method(D_METHOD("_on_custom_stage", "id"), &JVPickerUI::_on_custom_stage);
	ClassDB::bind_method(D_METHOD("_on_game_stage_picked", "id"), &JVPickerUI::_on_game_stage_picked);
	ClassDB::bind_method(D_METHOD("_on_game_tile", "id"), &JVPickerUI::_on_game_tile);
	ClassDB::bind_method(D_METHOD("_on_stage_closed"), &JVPickerUI::_on_stage_closed);
	ClassDB::bind_method(D_METHOD("_on_ball_tile", "id", "game"), &JVPickerUI::_on_ball_tile);
	ClassDB::bind_method(D_METHOD("_on_ball_done"), &JVPickerUI::_on_ball_done);
	ClassDB::bind_method(D_METHOD("_on_dim_gui", "event"), &JVPickerUI::_on_dim_gui);
	ClassDB::bind_method(D_METHOD("_on_tile_hover", "btn", "on"), &JVPickerUI::_on_tile_hover);
	ClassDB::bind_method(D_METHOD("ball_picker_open"), &JVPickerUI::ball_picker_open);
	ClassDB::bind_method(D_METHOD("inject_stage_tiles"), &JVPickerUI::inject_stage_tiles);
}

Object *JVPickerUI::obj(const Variant &v) {
	return v.get_type() == Variant::OBJECT ? v.operator Object *() : nullptr;
}

void JVPickerUI::_ready() {
	set_process_priority(1001);
	set_process_unhandled_input(true);
}

Node *JVPickerUI::menu() const {
	return Object::cast_to<Node>(ObjectDB::get_instance(menu_id_));
}



static JVModAPI *core_api() {
	Engine *e = Engine::get_singleton();
	return e->has_singleton("JVModAPI") ? Object::cast_to<JVModAPI>(e->get_singleton("JVModAPI")) : nullptr;
}
template <typename... T>
static Variant TEX(const char *cap, const char *method, const T &...a) {
	JVModAPI *api = core_api();
	if (!api || !jvm_core_ui_io().api()) return Variant();
	Array arr;
	(arr.push_back(Variant(a)), ...);
	return api->core_forward("core_ui", cap, "textures", method, arr);
}
template <typename... T>
static Variant GOOFY(const char *cap, const char *method, const T &...a) {
	JVModAPI *api = core_api();
	if (!api || !jvm_core_ui_io().api()) return Variant();
	Array arr;
	(arr.push_back(Variant(a)), ...);
	return api->core_forward("core_ui", cap, "goofy", method, arr);
}
static void apply_cosmetics_now() { TEX("stage.pick", "apply_picked_cosmetics"); }

Node *JVPickerUI::tex() const {
	if (!get_tree()) return nullptr;
	JVModAPI *api = core_api();
	return api ? api->core_library("textures") : nullptr;
}

bool JVPickerUI::load_ui_style() {
	if (ui_style_.is_valid()) return true;
	TypedArray<Dictionary> gl = ProjectSettings::get_singleton()->get_global_class_list();
	for (int i = 0; i < gl.size(); i++) {
		Dictionary d = gl[i];
		if (String(d.get("class", "")) == "UiStyle") {
			ui_style_ = jvm_core_ui_io().resource("ui.style", String(d.get("path", "")));
			break;
		}
	}
	if (ui_style_.is_null()) return false;
	ui_consts_ = ui_style_->get_script_constant_map();
	return ui_consts_.has("PRIMARY") && ui_consts_.has("INK");
}

void JVPickerUI::style_button(Button *b, const String &kind, int size) {
	if (ui_style_.is_valid()) jvm_core_ui_io().call("ui.style", ui_style_, "style_button", b, kind, size);
}

void JVPickerUI::sfx(const String &name) {
	if (ui_style_.is_valid()) jvm_core_ui_io().call("ui.style", ui_style_, "sfx", name);
}

void JVPickerUI::connect_once(Object *from, const StringName &sig, const Callable &cb, const String &meta) {
	if (!from) return;
	if (from->has_meta(meta)) return;
	jvm_core_ui_io().connect("stage.picker", from, sig, cb);
	from->set_meta(meta, true);
}

String JVPickerUI::game_ball_label(const String &id) const {
	for (int i = 0; i < kGameBallCount; i++)
		if (id == kGameBalls[i][0]) return kGameBalls[i][1];
	return id;
}

Ref<Texture2D> JVPickerUI::game_ball_thumb(const String &id) {
	for (int i = 0; i < kGameBallCount; i++) {
		if (id != kGameBalls[i][0]) continue;
		String path = kGameBalls[i][2];
		Ref<Resource> r = jvm_core_ui_io().resource("stage.picker", path);
		if (r.is_valid()) return Ref<Texture2D>(Object::cast_to<Texture2D>(r.ptr()));
	}
	return Ref<Texture2D>();
}

void JVPickerUI::_process(double delta) {
	JVM_PROF("core.picker_process");
	t_ += delta;
	if (t_ < 0.2) {


		return;
	}
	t_ = 0;
	Node *scene = jvm::current_scene(this);
	if (!scene || !scene->has_meta("jvm_menu_ui")) {
		if (menu_id_ && (!scene || scene->get_instance_id() != menu_id_)) {
			menu_id_ = 0;
			ball_hooked_ = stage_hooked_ = picker_signals_ = false;
			ball_btn_id_ = stage_btn_id_ = hooked_picker_id_ = 0;
		}
		return;
	}
	if (scene->get_instance_id() != menu_id_) {
		menu_id_ = scene->get_instance_id();
		ball_hooked_ = stage_hooked_ = picker_signals_ = false;
		ball_btn_id_ = stage_btn_id_ = hooked_picker_id_ = 0;
		hook_menu(scene);
	} else if (!ball_hooked_ || !stage_hooked_) {
		hook_menu(scene);
	}
	inject_stage_tiles();
	update_pills(scene);
}

void JVPickerUI::hook_menu(Node *m) {
	if (!m) return;
	Node *bar = Object::cast_to<Node>(obj(jvm_core_ui_io().get("ui.menu", m, "_bottom_bar")));
	if (!bar) return;
	for (int i = 0; i < bar->get_child_count(); i++) {
		Button *b = Object::cast_to<Button>(bar->get_child(i));
		if (!b) continue;
		String t = b->get_text();
		if (!ball_hooked_ && t.begins_with("ball:")) hook_ball_button(b);
		if (!stage_hooked_ && t.begins_with("stage:")) hook_stage_button(b);
	}
	if (Node *tl = tex()) {
		if (ball_current_.is_empty()) {
			String picked = TEX("packs.query", "get_picked_ball");
			if (!picked.is_empty()) ball_current_ = picked;
			else ball_current_ = String(jvm_core_ui_io().get("stage.picker", m, "_ball_skin"));
		}
	}
}

void JVPickerUI::hook_ball_button(Button *b) {

	TypedArray<Dictionary> cons = b->get_signal_connection_list("pressed");
	for (int i = 0; i < cons.size(); i++) {
		Dictionary d = cons[i];
		Callable c = d["callable"];
		jvm_core_ui_io().disconnect("stage.picker", b, "pressed", c);
	}
	jvm_core_ui_io().connect("stage.picker", b, "pressed", Callable(this, "_on_ball_btn"));
	ball_btn_id_ = b->get_instance_id();
	ball_hooked_ = true;
	jvm::log("menu", "ball pill: cycle replaced with a tile picker");
}

void JVPickerUI::hook_stage_button(Button *b) {

	jvm_core_ui_io().connect("stage.picker", b, "pressed", Callable(this, "_on_stage_btn"));
	stage_btn_id_ = b->get_instance_id();
	stage_hooked_ = true;
	jvm::log("menu", "stage pill: custom tiles will be added after Outer Space");
}

void JVPickerUI::_on_ball_btn() { open_ball_picker(); }
void JVPickerUI::_on_stage_btn() { call_deferred("inject_stage_tiles"); }
void JVPickerUI::_on_custom_stage(const String &id) { on_custom_stage_pressed(id); }
void JVPickerUI::_on_game_stage_picked(const String &id) { on_game_stage_picked(id); }
void JVPickerUI::_on_game_tile(const String &id) { on_game_tile_pressed(id); }
void JVPickerUI::_on_stage_closed() { on_stage_picker_closed(); }
void JVPickerUI::_on_ball_tile(const String &id, bool game) { select_ball(id, game); }
void JVPickerUI::_on_ball_done() { close_ball_picker(); }

void JVPickerUI::update_pills(Node *m) {
	if (!m) return;
	Node *tl = tex();
	String stage_pick = tl ? String(TEX("packs.query", "get_picked_stage")) : String();
	String ball_pick = tl ? String(TEX("packs.query", "get_picked_ball")) : String();
	if (Button *sb = Object::cast_to<Button>(ObjectDB::get_instance(stage_btn_id_))) {
		if (!stage_pick.is_empty()) {
			Dictionary info = TEX("packs.query", "pack_info", stage_pick);
			String label = String(info.get("label", info.get("name", stage_pick))).to_lower();
			String text = "stage: " + label;

			if (Node *gc = m->get_tree() ? jvm_core_ui_io().node("stage.picker", m->get_tree()->get_root(), "/root/GameConfig") : nullptr) {
				int tod = int(jvm_core_ui_io().get("stage.picker", gc, "menu_tod"));
				PackedStringArray variants = info.get("variants", PackedStringArray());
				if (tod > 0 && tod < variants.size()) text += " " + String(variants[tod]).to_lower();
				else if (tod > 0) {

					String base = info.get("base_stage", "grass");


					if (!stage_class_looked_) {
						stage_class_looked_ = true;
						TypedArray<Dictionary> gl = ProjectSettings::get_singleton()->get_global_class_list();
						for (int i = 0; i < gl.size(); i++) {
							Dictionary d = gl[i];
							if (String(d.get("class", "")) != "Stage") continue;
							stage_class_path_ = String(d.get("path", ""));
							stage_class_found_ = true;
							break;
						}
					}
					if (stage_class_found_) {
						Ref<Script> st = jvm_core_ui_io().resource("stage.picker", stage_class_path_);
						if (st.is_valid() && st->has_method("tod_labels")) {
							Array labels = jvm_core_ui_io().call("stage.picker", st, "tod_labels", base);
							if (tod < labels.size()) text += " " + String(labels[tod]).to_lower();
						}
					}
				}
			}
			if (sb->get_text() != text) sb->set_text(text);
		}
	}
	if (Button *bb = Object::cast_to<Button>(ObjectDB::get_instance(ball_btn_id_))) {
		String text;
		if (!ball_pick.is_empty()) {
			Dictionary info = TEX("packs.query", "pack_info", ball_pick);
			text = "ball: " + String(info.get("label", info.get("name", ball_pick))).to_lower();
		} else {
			String skin = String(jvm_core_ui_io().get("stage.picker", m, "_ball_skin"));
			text = "ball: " + game_ball_label(skin);
		}
		if (bb->get_text() != text) bb->set_text(text);
	}
}

void JVPickerUI::inject_stage_tiles() {
	Node *m = menu();
	if (!m) return;
	Node *picker = Object::cast_to<Node>(obj(jvm_core_ui_io().get("ui.menu", m, "_fp_picker")));
	bool is_open = picker && picker->has_method("is_open") && bool(jvm_core_ui_io().call("stage.picker", picker, "is_open"));
	if (!is_open) { stage_open_seen_ = false; highlight_init_ = false; return; }
	if (!load_ui_style()) return;
	GridContainer *grid = Object::cast_to<GridContainer>(obj(jvm_core_ui_io().get("stage.picker", picker, "_grid")));
	if (!grid) return;

	Node *tl = tex();
	if (!stage_open_seen_) {
		if (tl && tl->has_method("rescan_picker_packs")) TEX("packs.rescan", "rescan_picker_packs");
		stage_open_seen_ = true;
	}
	Array packs = tl ? Array(TEX("packs.query", "list_picker_packs", "stage")) : Array();

	connect_once(picker, "stage_picked", Callable(this, "_on_game_stage_picked"), "jvm_picker_stage_picked");
	connect_once(picker, "closed", Callable(this, "_on_stage_closed"), "jvm_picker_stage_closed");
	hooked_picker_id_ = picker->get_instance_id();




	Dictionary existing_tiles = jvm_core_ui_io().get("stage.picker", picker, "_tiles");
	Array tkeys = existing_tiles.keys();
	for (int i = 0; i < tkeys.size(); i++) {
		String gid = tkeys[i];
		Button *tb = Object::cast_to<Button>(obj(existing_tiles[gid]));
		if (!tb || tb->has_meta("jvm_stage_pack")) continue;
		connect_once(tb, "pressed", Callable(this, "_on_game_tile").bind(gid), String("jvm_game_tile_") + gid);
	}


	std::vector<Node *> stale;
	for (int i = 0; i < grid->get_child_count(); i++) {
		Node *c = grid->get_child(i);
		if (!c->has_meta("jvm_stage_pack")) continue;
		String id = c->get_meta("jvm_stage_pack");
		bool found = false;
		for (int p = 0; p < packs.size(); p++) {
			if (String(Dictionary(packs[p]).get("id", "")) == id) { found = true; break; }
		}
		if (!found) stale.push_back(c);
	}
	for (Node *c : stale) {
		grid->remove_child(c);
		c->queue_free();
	}

	Dictionary tiles = jvm_core_ui_io().get("stage.picker", picker, "_tiles");
	for (int p = 0; p < packs.size(); p++) {
		Dictionary info = packs[p];
		String id = info.get("id", "");
		if (id.is_empty()) continue;
		bool exists = false;
		for (int i = 0; i < grid->get_child_count(); i++) {
			Node *c = grid->get_child(i);
			if (c->has_meta("jvm_stage_pack") && String(c->get_meta("jvm_stage_pack")) == id) { exists = true; break; }
		}
		if (exists) continue;
		String title = info.get("label", info.get("name", id));
		Ref<Texture2D> thumb = info.get("preview", Variant());
		Button *b = make_tile(id, title, thumb, Callable(this, "_on_custom_stage").bind(id));
		b->set_meta("jvm_stage_pack", id);
		grid->add_child(b);
		tiles[id] = b;
	}
	jvm_core_ui_io().set("stage.picker", picker, "_tiles", tiles);

	String picked = tl ? String(TEX("packs.query", "get_picked_stage")) : String();
	if (!game_tile_picked_) {
		if (pending_stage_.is_empty() && !committed_stage_.is_empty()) pending_stage_ = committed_stage_;
		if (pending_stage_.is_empty() && !picked.is_empty()) pending_stage_ = committed_stage_ = picked;
		if (!pending_stage_.is_empty()) refresh_stage_preview(pending_stage_, false);
	}
	if (!highlight_init_) {
		highlight_init_ = true;
		highlight_ = committed_stage_.is_empty() ? picked : committed_stage_;
	}
	ensure_ball_override_row(picker);
	update_ball_override_row(picker);
}

void JVPickerUI::on_custom_stage_pressed(const String &id) {
	game_tile_picked_ = false;
	committed_stage_ = id;
	highlight_ = id;
	refresh_stage_preview(id, true);
	if (Node *m = menu()) update_ball_override_row(Object::cast_to<Node>(obj(jvm_core_ui_io().get("ui.menu", m, "_fp_picker"))));

	if (Node *tl = tex()) {
		if (tl->has_method("set_picked_stage")) TEX("stage.pick", "set_picked_stage", id);
	}
}

void JVPickerUI::on_game_tile_pressed(const String &id) {
	jvm::log("menu", "game stage tile pressed: " + id);
	game_tile_picked_ = true;
	highlight_ = "";
	pending_stage_ = "";
	committed_stage_ = "";
	if (Node *tl = tex()) {
		if (tl->has_method("set_picked_stage")) TEX("stage.pick", "set_picked_stage", String());
	}
	if (Node *m = menu()) update_ball_override_row(Object::cast_to<Node>(obj(jvm_core_ui_io().get("ui.menu", m, "_fp_picker"))));
}

void JVPickerUI::refresh_stage_preview(const String &id, bool from_user) {
	Node *m = menu();
	Node *tl = tex();
	if (!m || !tl) return;
	Dictionary info = TEX("packs.query", "pack_info", id);
	if (info.is_empty()) return;
	pending_stage_ = id;
	String base = info.get("base_stage", "grass");
	Node *picker = Object::cast_to<Node>(obj(jvm_core_ui_io().get("ui.menu", m, "_fp_picker")));
	if (picker) {
		jvm_core_ui_io().set("stage.picker", picker, "_current", id);
		if (picker->has_method("_paint_tiles")) jvm_core_ui_io().call("stage.picker", picker, "_paint_tiles");
		Node *preview = Object::cast_to<Node>(obj(jvm_core_ui_io().get("stage.picker", picker, "_preview")));
		if (preview && preview->has_method("set_stage")) {
			jvm_core_ui_io().call("stage.picker", preview, "set_stage", base);
			if (Label *title = Object::cast_to<Label>(obj(jvm_core_ui_io().get("stage.picker", preview, "_title"))))
				title->set_text(String(info.get("label", info.get("name", id))));
			if (TextureRect *thumb = Object::cast_to<TextureRect>(obj(jvm_core_ui_io().get("stage.picker", preview, "_thumb")))) {
				Ref<Texture2D> texv = info.get("preview", Variant());
				if (texv.is_valid()) {
					thumb->set_texture(texv);
					thumb->set_visible(true);
				}
			}
		}
		Array fpp = jvm_core_ui_io().get("stage.picker", m, "_fp_pick");
		int tod = fpp.size() > 1 ? int(fpp[1]) : 0;
		Array np;
		np.append(base);
		np.append(tod);
		jvm_core_ui_io().set("stage.picker", m, "_fp_pick", np);
	}
	if (from_user) sfx("toggle_on");
}

void JVPickerUI::on_game_stage_picked(const String &id) {



	jvm::log("menu", "stage_picked signal: " + id + " pending=" + pending_stage_ + " committed=" + committed_stage_);
}

void JVPickerUI::on_stage_picker_closed() {
	Node *m = menu();
	Node *tl = tex();
	String want = pending_stage_;
	if (want.is_empty()) want = committed_stage_;
	if (!game_tile_picked_ && want.is_empty() && tl)
		want = TEX("packs.query", "get_picked_stage");
	if (game_tile_picked_) want = "";
	jvm::log("menu", String("stage picker closed want=") + (want.is_empty() ? String("(game)") : want)
		+ " pending=" + pending_stage_ + " committed=" + committed_stage_
		+ " game_tile=" + (game_tile_picked_ ? "y" : "n"));
	if (tl && tl->has_method("set_picked_stage")) {
		TEX("stage.pick", "set_picked_stage", want);
		if (tl->has_method("apply_picked_cosmetics")) callable_mp_static(&apply_cosmetics_now).call_deferred();
	}
	pending_stage_ = want;
	committed_stage_ = want;
	game_tile_picked_ = false;

	update_pills(m);
}

void JVPickerUI::open_ball_picker() {
	Node *m = menu();
	if (!m || !load_ui_style()) return;
	if (Node *tl = tex()) {
		if (tl->has_method("rescan_picker_packs")) TEX("packs.rescan", "rescan_picker_packs");
		String picked = TEX("packs.query", "get_picked_ball");
		if (!picked.is_empty()) ball_current_ = picked;
		else if (ball_current_.is_empty()) ball_current_ = String(jvm_core_ui_io().get("stage.picker", m, "_ball_skin"));
	}
	CanvasLayer *layer = Object::cast_to<CanvasLayer>(ObjectDB::get_instance(ball_layer_id_));
	if (!layer) {
		build_ball_picker(m);
		layer = Object::cast_to<CanvasLayer>(ObjectDB::get_instance(ball_layer_id_));
	}
	rebuild_ball_grid();
	if (Control *root = Object::cast_to<Control>(ObjectDB::get_instance(ball_root_id_))) root->set_visible(true);
	sfx("open");
}

void JVPickerUI::close_ball_picker() {
	Control *root = Object::cast_to<Control>(ObjectDB::get_instance(ball_root_id_));
	if (!root || !root->is_visible()) return;
	root->set_visible(false);
	sfx("close");
	if (Button *b = Object::cast_to<Button>(ObjectDB::get_instance(ball_btn_id_)))
		if (b->is_visible_in_tree()) b->grab_focus();
}

bool JVPickerUI::ball_picker_open() const {
	Control *root = Object::cast_to<Control>(ObjectDB::get_instance(ball_root_id_));
	return root && root->is_visible();
}

void JVPickerUI::_unhandled_input(const Ref<InputEvent> &event) {
	if (event.is_null()) return;
	if (ball_picker_open() && event->is_action_pressed("ui_cancel")) {
		close_ball_picker();
		if (Viewport *vp = get_viewport()) vp->set_input_as_handled();
	}
}

void JVPickerUI::build_ball_picker(Node *parent) {
	CanvasLayer *layer = memnew(CanvasLayer);
	layer->set_name("JvmBallPicker");
	layer->set_layer(85);
	parent->add_child(layer);
	ball_layer_id_ = layer->get_instance_id();

	Control *root = memnew(Control);
	root->set_name("Root");
	root->set_anchors_preset(Control::PRESET_FULL_RECT);
	root->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	root->set_visible(false);
	layer->add_child(root);
	ball_root_id_ = root->get_instance_id();

	Color ink = ui_consts_["INK"];
	ColorRect *dim = memnew(ColorRect);
	dim->set_color(Color(ink, 0.45f));
	dim->set_anchors_preset(Control::PRESET_FULL_RECT);
	dim->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	dim->connect("gui_input", Callable(this, "_on_dim_gui"));
	root->add_child(dim);

	CenterContainer *centre = memnew(CenterContainer);
	centre->set_anchors_preset(Control::PRESET_FULL_RECT);
	centre->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	root->add_child(centre);

	PanelContainer *card = memnew(PanelContainer);
	card->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	if (ui_style_.is_valid()) {
		Ref<StyleBox> box = jvm_core_ui_io().call("ui.style", ui_style_, "card_box");
		if (box.is_valid()) box = box->duplicate();
		Ref<StyleBoxFlat> flat = box;
		if (flat.is_valid()) {
			int radius = int(ui_consts_.get("RADIUS_SOFT", 18));
			flat->set_corner_radius_all(radius);
			flat->set_content_margin(SIDE_LEFT, 22);
			flat->set_content_margin(SIDE_RIGHT, 22);
			flat->set_content_margin(SIDE_TOP, 18);
			flat->set_content_margin(SIDE_BOTTOM, 20);
			card->add_theme_stylebox_override("panel", flat);
		} else if (box.is_valid()) {
			card->add_theme_stylebox_override("panel", box);
		}
	}
	centre->add_child(card);

	VBoxContainer *v = memnew(VBoxContainer);
	v->add_theme_constant_override("separation", 14);
	card->add_child(v);


	float banner_w = kCols * kTile.x + (kCols - 1) * 10.0f;
	Control *banner = memnew(Control);
	banner->set_custom_minimum_size(Vector2(banner_w, 86));
	banner->set_clip_contents(true);
	v->add_child(banner);

	TextureRect *bthumb = memnew(TextureRect);
	bthumb->set_anchors_preset(Control::PRESET_FULL_RECT);
	bthumb->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	bthumb->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
	bthumb->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	banner->add_child(bthumb);
	ball_banner_thumb_id_ = bthumb->get_instance_id();

	ColorRect *wash = memnew(ColorRect);
	wash->set_anchors_preset(Control::PRESET_FULL_RECT);
	wash->set_color(Color(0, 0, 0, 0.25f));
	wash->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	banner->add_child(wash);

	Label *btitle = memnew(Label);
	btitle->set_anchors_preset(Control::PRESET_FULL_RECT);
	btitle->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	btitle->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	if (ui_style_.is_valid() && ui_consts_.has("FONT_BLACK"))
		btitle->add_theme_font_override("font", Ref<Font>(jvm_core_ui_io().call("ui.style", ui_style_, "font", ui_consts_["FONT_BLACK"])));
	btitle->add_theme_font_size_override("font_size", 34);
	btitle->add_theme_color_override("font_color", Color(1, 1, 1));
	btitle->add_theme_color_override("font_outline_color", ink);
	btitle->add_theme_constant_override("outline_size", 9);
	btitle->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	banner->add_child(btitle);
	ball_banner_title_id_ = btitle->get_instance_id();

	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(kCols);
	grid->add_theme_constant_override("h_separation", 10);
	grid->add_theme_constant_override("v_separation", 10);
	v->add_child(grid);
	ball_grid_id_ = grid->get_instance_id();

	Button *done = memnew(Button);
	done->set_text("done");
	style_button(done, "primary", 16);
	done->set_custom_minimum_size(Vector2(220, 0));
	done->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	done->connect("pressed", Callable(this, "_on_ball_done"));
	v->add_child(done);
}

void JVPickerUI::rebuild_ball_grid() {
	GridContainer *grid = Object::cast_to<GridContainer>(ObjectDB::get_instance(ball_grid_id_));
	if (!grid) return;
	while (grid->get_child_count()) {
		Node *c = grid->get_child(0);
		grid->remove_child(c);
		c->queue_free();
	}

	for (int i = 0; i < kGameBallCount; i++) {
		String id = kGameBalls[i][0];
		Button *b = make_tile(id, kGameBalls[i][1], game_ball_thumb(id),
			Callable(this, "_on_ball_tile").bind(id, true));
		b->set_meta("jvm_ball_id", id);
		b->set_meta("jvm_ball_game", true);
		grid->add_child(b);
	}
	Node *tl = tex();
	Array packs = tl ? Array(TEX("packs.query", "list_picker_packs", "ball")) : Array();
	for (int p = 0; p < packs.size(); p++) {
		Dictionary info = packs[p];
		String id = info.get("id", "");
		if (id.is_empty()) continue;
		String title = info.get("label", info.get("name", id));
		Ref<Texture2D> thumb = info.get("preview", Variant());
		Button *b = make_tile(id, title, thumb, Callable(this, "_on_ball_tile").bind(id, false));
		b->set_meta("jvm_ball_id", id);
		b->set_meta("jvm_ball_game", false);
		grid->add_child(b);
	}
	paint_ball_tiles();

	bool game = false;
	for (int i = 0; i < kGameBallCount; i++) if (ball_current_ == kGameBalls[i][0]) game = true;
	if (game) set_ball_banner(game_ball_label(ball_current_), game_ball_thumb(ball_current_));
	else if (tl) {
		Dictionary info = TEX("packs.query", "pack_info", ball_current_);
		set_ball_banner(info.get("label", info.get("name", ball_current_)), info.get("preview", Variant()));
	}
}

void JVPickerUI::select_ball(const String &id, bool game) {
	ball_current_ = id;
	Node *m = menu();
	Node *tl = tex();
	if (game) {
		if (m) apply_game_ball(m, id);
	} else if (tl && tl->has_method("set_picked_ball")) {
		TEX("ball.pick", "set_picked_ball", id);
	}
	paint_ball_tiles();
	if (game) set_ball_banner(game_ball_label(id), game_ball_thumb(id));
	else if (tl) {
		Dictionary info = TEX("packs.query", "pack_info", id);
		set_ball_banner(info.get("label", info.get("name", id)), info.get("preview", Variant()));
	}
	sfx("toggle_on");
	update_pills(m);
}

void JVPickerUI::apply_game_ball(Node *m, const String &id) {
	if (!m) return;
	jvm_core_ui_io().set("stage.picker", m, "_ball_skin", id);
	if (Node *ball = Object::cast_to<Node>(obj(jvm_core_ui_io().get("stage.picker", m, "ball")))) {
		if (ball->has_method("set_ball_skin")) jvm_core_ui_io().call("ball.skin", ball, "set_ball_skin", id);
	}
	SceneTree *tree = m->get_tree();
	if (tree) {
		if (Node *net = jvm_core_ui_io().node("net.party_ball", tree->get_root(), "/root/Net")) {
			if (net->has_method("send_party_ball")) jvm_core_ui_io().call("net.party_ball", net, "send_party_ball", id);
		}
		if (Node *gc = jvm_core_ui_io().node("stage.picker", tree->get_root(), "/root/GameConfig")) {
			jvm_core_ui_io().set("ball.pick", gc, "menu_ball", id);
			if (gc->has_method("save_settings")) jvm_core_ui_io().call("ball.pick", gc, "save_settings");
		}
		if (Node *ach = jvm_core_ui_io().node("ui.notify", tree->get_root(), "/root/Achievements")) {
			if (ach->has_method("notify")) jvm_core_ui_io().call("ui.notify", ach, "notify", "menu_ball");
		}
	}

	if (Node *tl = tex()) {
		if (tl->has_method("set_picked_ball")) TEX("ball.pick", "set_picked_ball", "");
		if (tl->has_method("apply_picked_cosmetics")) TEX("stage.pick", "apply_picked_cosmetics");
	}
}

void JVPickerUI::set_ball_banner(const String &title, const Ref<Texture2D> &thumb) {
	if (Label *l = Object::cast_to<Label>(ObjectDB::get_instance(ball_banner_title_id_))) l->set_text(title);
	if (TextureRect *r = Object::cast_to<TextureRect>(ObjectDB::get_instance(ball_banner_thumb_id_))) {
		r->set_texture(thumb);
		r->set_visible(thumb.is_valid());
	}
}

void JVPickerUI::paint_ball_tiles() {
	GridContainer *grid = Object::cast_to<GridContainer>(ObjectDB::get_instance(ball_grid_id_));
	if (!grid) return;
	for (int i = 0; i < grid->get_child_count(); i++) {
		Button *b = Object::cast_to<Button>(grid->get_child(i));
		if (!b) continue;
		paint_tile(b, String(b->get_meta("jvm_ball_id", "")) == ball_current_);
	}
}

Button *JVPickerUI::make_tile(const String &id, const String &title, const Ref<Texture2D> &thumb, const Callable &on_press) {
	(void)id;
	Color ink = ui_consts_.get("INK", Color(0.1, 0.1, 0.14));
	Button *b = memnew(Button);
	b->set_custom_minimum_size(kTile);
	b->set_focus_mode(Control::FOCUS_ALL);
	b->set_clip_contents(true);
	b->set_default_cursor_shape(Control::CURSOR_POINTING_HAND);
	b->set_tooltip_text(title);
	for (const char *st : { "normal", "hover", "pressed", "focus", "disabled" }) {
		Ref<StyleBoxFlat> sb;
		sb.instantiate();
		sb->set_bg_color(Color("6fb7ff"));
		sb->set_corner_radius_all(14);
		b->add_theme_stylebox_override(st, sb);
	}
	TextureRect *pic = memnew(TextureRect);
	pic->set_anchors_preset(Control::PRESET_FULL_RECT);
	pic->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	pic->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
	pic->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	if (thumb.is_valid()) pic->set_texture(thumb);
	b->add_child(pic);
	b->set_meta("thumb", pic);

	Label *name = memnew(Label);
	name->set_text(title);
	name->set_anchors_preset(Control::PRESET_FULL_RECT);
	name->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	name->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	name->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	if (ui_style_.is_valid() && ui_consts_.has("FONT_BLACK"))
		name->add_theme_font_override("font", Ref<Font>(jvm_core_ui_io().call("ui.style", ui_style_, "font", ui_consts_["FONT_BLACK"])));
	name->add_theme_font_size_override("font_size", 19);
	name->add_theme_color_override("font_color", Color(1, 1, 1));
	name->add_theme_color_override("font_outline_color", ink);
	name->add_theme_constant_override("outline_size", 7);
	name->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	b->add_child(name);

	Panel *ring = memnew(Panel);
	ring->set_anchors_preset(Control::PRESET_FULL_RECT);
	ring->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	b->add_child(ring);
	b->set_meta("ring", ring);

	Panel *foc = memnew(Panel);
	Ref<StyleBoxFlat> fsb;
	fsb.instantiate();
	fsb->set_bg_color(Color(0, 0, 0, 0));
	fsb->set_corner_radius_all(10);
	fsb->set_border_width_all(4);
	fsb->set_border_color(ui_consts_.get("SECONDARY", Color(0.45, 0.75, 1)));
	foc->add_theme_stylebox_override("panel", fsb);
	foc->set_anchors_preset(Control::PRESET_FULL_RECT);
	foc->set_offset(SIDE_LEFT, 5);
	foc->set_offset(SIDE_TOP, 5);
	foc->set_offset(SIDE_RIGHT, -5);
	foc->set_offset(SIDE_BOTTOM, -5);
	foc->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	foc->set_visible(false);
	b->add_child(foc);
	b->set_meta("focus", foc);

	if (ui_style_.is_valid()) jvm_core_ui_io().call("ui.style", ui_style_, "hover_sound", b);
	b->connect("mouse_entered", Callable(this, "_on_tile_hover").bind(b, true));
	b->connect("mouse_exited", Callable(this, "_on_tile_hover").bind(b, false));
	b->connect("focus_entered", Callable(this, "_on_tile_hover").bind(b, true));
	b->connect("focus_exited", Callable(this, "_on_tile_hover").bind(b, false));
	paint_tile(b, false);
	b->connect("pressed", on_press);
	return b;
}

void JVPickerUI::_on_dim_gui(const Ref<InputEvent> &event) {
	Ref<InputEventMouseButton> mb = event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT)
		close_ball_picker();
}

void JVPickerUI::_on_tile_hover(Object *btn, bool on) {
	Button *b = Object::cast_to<Button>(btn);
	if (!b) return;
	b->set_meta("hover", on);
	String id = b->get_meta("jvm_ball_id", b->get_meta("jvm_stage_pack", ""));
	bool selected = (!id.is_empty() && (id == ball_current_ || id == pending_stage_));
	paint_tile(b, selected);

	Node *m = menu();
	if (!m) return;
	Node *picker = Object::cast_to<Node>(obj(jvm_core_ui_io().get("ui.menu", m, "_fp_picker")));
	if (picker && picker->has_method("_paint_tiles") && b->has_meta("jvm_stage_pack"))
		jvm_core_ui_io().call("stage.picker", picker, "_paint_tiles");
}

void JVPickerUI::paint_tile(Button *b, bool on) {
	if (!b) return;
	Color ink = ui_consts_.get("INK", Color(0.1, 0.1, 0.14));
	Color primary = ui_consts_.get("PRIMARY", Color("ffd23f"));
	bool lit = b->has_focus() || bool(b->get_meta("hover", false));
	b->set_modulate((lit && !b->is_disabled()) ? Color(1.1, 1.1, 1.1) : Color(1, 1, 1));
	if (Panel *ring = Object::cast_to<Panel>(obj(b->get_meta("ring", Variant())))) {
		Ref<StyleBoxFlat> sb;
		sb.instantiate();
		sb->set_bg_color(Color(0, 0, 0, 0));
		sb->set_corner_radius_all(14);
		sb->set_border_width_all(on ? 5 : 2);
		sb->set_border_color(on ? primary : Color(ink, 0.25f));
		ring->add_theme_stylebox_override("panel", sb);
	}
	if (Control *foc = Object::cast_to<Control>(obj(b->get_meta("focus", Variant()))))
		foc->set_visible(b->has_focus());
}








Node *JVPickerUI::goofy() const {
	if (!get_tree()) return nullptr;
	JVModAPI *api = core_api();
	Node *g = api ? api->core_library("goofy") : nullptr;
	return g && g->has_method("set_ball_mode") && g->has_method("api_version") ? g : nullptr;
}

bool JVPickerUI::ball_override_visible() const {
	Control *row = Object::cast_to<Control>(ObjectDB::get_instance(ball_row_id_));
	return row && row->is_visible_in_tree();
}

void JVPickerUI::ensure_ball_override_row(Node *picker) {
	if (!picker || !goofy()) return;
	if (ball_row_id_ && ball_row_picker_ == picker->get_instance_id() && ObjectDB::get_instance(ball_row_id_)) return;
	GridContainer *grid = Object::cast_to<GridContainer>(obj(jvm_core_ui_io().get("stage.picker", picker, "_grid")));
	Node *v = grid ? grid->get_parent() : nullptr;
	if (!v) return;
	VBoxContainer *row = memnew(VBoxContainer);
	row->set_name("JvmBallOverride");
	row->add_theme_constant_override("separation", 4);
	row->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	HBoxContainer *h = memnew(HBoxContainer);
	h->add_theme_constant_override("separation", 10);
	h->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	Label *l = memnew(Label);
	l->set_text("ball");
	l->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	if (ui_style_.is_valid()) jvm_core_ui_io().call("ui.style", ui_style_, "style_label", l, "body");
	h->add_child(l);
	auto mk = [&](const char *text, const char *mode, const char *tip) {
		Button *b = memnew(Button);
		b->set_text(text);
		b->set_tooltip_text(tip);
		b->set_custom_minimum_size(Vector2(120, 0));
		b->set_focus_mode(Control::FOCUS_ALL);
		b->connect("pressed", Callable(this, "_on_ball_mode").bind(String(mode)));
		h->add_child(b);
		return b;
	};
	ball_normal_id_ = mk("normal", "normal", "this map's ball settings plus your goofy settings")->get_instance_id();
	ball_fixed_id_ = mk("fixed", "fixed", "the game's own ball on this map: physics, hitboxes and hit range")->get_instance_id();
	row->add_child(h);
	Label *cap = memnew(Label);
	cap->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	if (ui_style_.is_valid()) jvm_core_ui_io().call("ui.style", ui_style_, "style_label", cap, "small");
	row->add_child(cap);
	ball_caption_id_ = cap->get_instance_id();
	v->add_child(row);
	v->move_child(row, grid->get_index() + 1);
	row->set_visible(false);
	ball_row_id_ = row->get_instance_id();
	ball_row_picker_ = picker->get_instance_id();
	paint_ball_override();
	jvm::log("menu", "stage picker: Ball override row added (normal / fixed), shown on custom maps only");
}

void JVPickerUI::paint_ball_override() {
	Node *g = goofy();
	String mode = g ? String(GOOFY("goofy.query", "get_ball_mode")) : String("normal");
	if (mode.is_empty()) mode = "normal";
	Button *n = Object::cast_to<Button>(ObjectDB::get_instance(ball_normal_id_));
	Button *f = Object::cast_to<Button>(ObjectDB::get_instance(ball_fixed_id_));
	if (n && String(n->get_meta("jvm_kind", "")) != (mode == "normal" ? "primary" : "secondary")) {
		style_button(n, mode == "normal" ? "primary" : "secondary", 16); n->set_meta("jvm_kind", mode == "normal" ? "primary" : "secondary");
	}
	if (f && String(f->get_meta("jvm_kind", "")) != (mode == "fixed" ? "primary" : "secondary")) {
		style_button(f, mode == "fixed" ? "primary" : "secondary", 16); f->set_meta("jvm_kind", mode == "fixed" ? "primary" : "secondary");
	}
	if (Label *c = Object::cast_to<Label>(ObjectDB::get_instance(ball_caption_id_))) {
		String t = mode == "fixed" ? "fixed: the game's own ball on this map (physics, hitboxes, hit range)"
		                           : "normal: this map's ball + your goofy settings (offline only)";
		if (c->get_text() != t) c->set_text(t);
	}
}

void JVPickerUI::update_ball_override_row(Node *picker) {
	Control *row = Object::cast_to<Control>(ObjectDB::get_instance(ball_row_id_));
	if (!row || !picker) return;



	String cur = highlight_;
	bool custom = false;
	if (!cur.is_empty())
		if (Node *grid = Object::cast_to<Node>(obj(jvm_core_ui_io().get("stage.picker", picker, "_grid"))))
			for (int i = 0; i < grid->get_child_count() && !custom; i++)
				custom = String(grid->get_child(i)->get_meta("jvm_stage_pack", "")) == cur;
	if (row->is_visible() != custom) {
		row->set_visible(custom);
		jvm::log("menu", String("Ball override row ") + (custom ? "shown (custom map " + cur + ")" : "hidden (game map)"));
	}
	if (custom) paint_ball_override();
}

void JVPickerUI::_on_ball_mode(const String &mode) {
	Node *g = goofy();
	if (!g) return;
	GOOFY("goofy.ball_mode", "set_ball_mode", mode);
	sfx("toggle_on");
	paint_ball_override();
}
