#pragma once
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/script.hpp>

namespace godot {










class JVMenuUI : public Node {
	GDCLASS(JVMenuUI, Node)
	double t_ = 0.0;
	uint64_t menu_id_ = 0, credits_id_ = 0, discord_id_ = 0, mods_btn_id_ = 0, panel_id_ = 0;
	uint64_t skipped_menu_ = 0;
	int injections_ = 0;
	String last_skip_;
	uint64_t view_id_ = 0, status_id_ = 0, dev_btn_id_ = 0;
	double dev_btn_t_ = 0;
	String view_;
	bool modal_slot_ = false, overlay_open_ = false;
	int saves_ = 0;
	String last_save_msg_;
	Ref<Script> ui_select_script_;
	bool try_inject(Node *menu);
	void clear_view();
	void refresh_sidebar();
	Label *add_label(Control *parent, const String &text, int size, const String &name);
	Button *add_button(Control *parent, const String &text, const Callable &cb, const String &name);
	Control *find_focusable(Node *n);
	void add_field(Control *parent, const String &file, const PackedStringArray &keypath, const Variant &val, bool can_edit, int depth);
	void build_file_editor(Control *ed, const String &file, bool can_edit);
	void guard_frame();
	void suppress_cards(Node *menu, bool on);
	Object *ui_select_active();
	void skip(Node *menu, const String &why);
	Dictionary gate_status();

protected:
	static void _bind_methods();

public:
	void _ready() override;
	void _process(double delta) override;
	void show_list();
	void show_detail(const String &mod_id);
	String save_detail();
	String validate_and_save(const String &path, const String &text, bool live);
	PackedStringArray config_files(const String &mod_id);
	void _toggle_raw(uint64_t id);
	void _on_goofy_live(const Variant &value, PackedStringArray keypath);
	void _dev_profiler_toggle();
	void open_mods_panel();
	void close_mods_panel();
	String panel_text();
	Dictionary get_info() const;
};

}
