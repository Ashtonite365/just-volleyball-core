#pragma once
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {










class JVPickerUI : public Node {
	GDCLASS(JVPickerUI, Node)

	uint64_t menu_id_ = 0, ball_btn_id_ = 0, stage_btn_id_ = 0;
	uint64_t ball_layer_id_ = 0, ball_root_id_ = 0, ball_grid_id_ = 0;
	uint64_t ball_banner_title_id_ = 0, ball_banner_thumb_id_ = 0;
	uint64_t hooked_picker_id_ = 0;
	bool ball_hooked_ = false, stage_hooked_ = false, picker_signals_ = false, stage_open_seen_ = false;
	bool game_tile_picked_ = false;
	String pending_stage_;
	String committed_stage_;
	String ball_current_;


	uint64_t ball_row_id_ = 0, ball_normal_id_ = 0, ball_fixed_id_ = 0, ball_caption_id_ = 0, ball_row_picker_ = 0;
	String highlight_;
	bool highlight_init_ = false;
	Ref<Script> ui_style_;
	Dictionary ui_consts_;
	String stage_class_path_;
	bool stage_class_looked_ = false, stage_class_found_ = false;
	double t_ = 0.2;

	static Object *obj(const Variant &v);
	Node *menu() const;
	Node *tex() const;
	bool load_ui_style();
	void style_button(Button *b, const String &kind, int size);
	void sfx(const String &name);
	void hook_menu(Node *m);
	void hook_ball_button(Button *b);
	void hook_stage_button(Button *b);
	void update_pills(Node *m);
	void inject_stage_tiles();
	void on_custom_stage_pressed(const String &id);
	void refresh_stage_preview(const String &id, bool from_user);
	void on_game_stage_picked(const String &id);
	void on_game_tile_pressed(const String &id);
	void on_stage_picker_closed();
	void open_ball_picker();
	void close_ball_picker();
	void build_ball_picker(Node *parent);
	void rebuild_ball_grid();
	void select_ball(const String &id, bool game);
	void apply_game_ball(Node *m, const String &id);
	Button *make_tile(const String &id, const String &title, const Ref<Texture2D> &thumb, const Callable &on_press);
	void paint_tile(Button *b, bool on);
	void paint_ball_tiles();
	void set_ball_banner(const String &title, const Ref<Texture2D> &thumb);
	Ref<Texture2D> game_ball_thumb(const String &id);
	String game_ball_label(const String &id) const;
	Node *goofy() const;
	void ensure_ball_override_row(Node *picker);
	void update_ball_override_row(Node *picker);
	void paint_ball_override();
	void connect_once(Object *from, const StringName &sig, const Callable &cb, const String &meta);

protected:
	static void _bind_methods();

public:
	void _ready() override;
	void _process(double delta) override;
	void _unhandled_input(const Ref<InputEvent> &event) override;
	bool ball_picker_open() const;
	void _on_ball_btn();
	void _on_stage_btn();
	void _on_custom_stage(const String &id);
	void _on_game_stage_picked(const String &id);
	void _on_game_tile(const String &id);
	void _on_stage_closed();
	void _on_ball_tile(const String &id, bool game);
	void _on_ball_done();
	void _on_dim_gui(const Ref<InputEvent> &event);
	void _on_tile_hover(Object *btn, bool on);
	void _on_ball_mode(const String &mode);
	bool ball_override_visible() const;
};

}
