



#pragma once
#include "../gate/const_check.hpp"

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace godot {

class JVModAPI;

class JVDevTools : public Node {
	GDCLASS(JVDevTools, Node)
public:
	enum Role { Head, Tail };

private:
	Role role_ = Head;
	JVDevTools *host_ = nullptr;
	JVDevTools *tail_ = nullptr;
	static JVDevTools *singleton_;


	bool cfg_loaded_ = false;


#ifdef JVM_DEV_BUILD
	static constexpr bool kDevBuild = true;
#else
	static constexpr bool kDevBuild = false;
#endif
	bool hotkeys_ = kDevBuild, overlay_cfg_ = true, consts_audit_ = true;
	String hotkeys_source_ = "build";

	String profiler_key_ = "F6", console_key_ = "F11";
	double default_seconds_ = 2.0;


	struct Frame {
		uint64_t start = 0, proc_head = 0, proc_tail = 0, pre_draw = 0, post_draw = 0;
		std::vector<std::pair<uint64_t, uint64_t>> phys;
		double vp_cpu = -1, vp_gpu = -1, eng_proc = -1, eng_phys = -1, setup_cpu = -1;
		int64_t draw_calls = -1, objects = -1, nodes = -1;
	};
	bool capturing_ = false;
	double cap_seconds_ = 2.0;
	String cap_tag_;
	String cap_started_;
	uint64_t cap_t0_ = 0, cap_stop_ = 0;
	uint64_t pending_iter_ = 0, phys_open_ = 0;
	std::vector<std::pair<uint64_t, uint64_t>> pending_phys_;
	std::vector<Frame> frames_;
	bool measuring_vp_ = false;
	bool cap_overlay_ = false;
	Dictionary last_;
	String last_json_, last_csv_;
	int captures_ = 0;

	CanvasLayer *ov_layer_ = nullptr;
	Label *ov_label_ = nullptr;
	double ov_hide_t_ = 0;


	bool dbg_on_ = false;
	struct Trace { double t_ms; int64_t frame; String kind, name, detail; };
	std::deque<Trace> trace_;
	CanvasLayer *con_layer_ = nullptr;
	PanelContainer *con_panel_ = nullptr;
	RichTextLabel *con_out_ = nullptr;
	LineEdit *con_in_ = nullptr;


	struct Rec { jvm::consts::Finding f; int64_t count = 1; double first_ms = 0, last_ms = 0; };
	std::vector<Rec> findings_;
	jvm::consts::Expect expect_;
	bool match_active_ = false;
	double consts_t_ = 0, consts_inactive_t_ = 0;
	bool bypass_active_ = false;
	String adapter_source_;
	int64_t audits_ = 0, checks_ = 0;
	std::map<String, bool> warned_;

	JVModAPI *api() const;
	void load_cfg();
	void begin_frame(uint64_t now);
	void finish_capture();
	Dictionary build_capture(const std::vector<Dictionary> &taps, uint64_t stop_ns);
	void write_capture(Dictionary &cap);
	void show_overlay(const String &text, double hold_s);
	void ensure_console();
	String run_command(const String &line);
	jvm::consts::Context consts_context() const;
	jvm::consts::Live consts_live_struct(const Dictionary &d) const;
	void note(const std::vector<jvm::consts::Finding> &fs);
	bool node_is_ours(Node *n) const;
	Dictionary node_info(Node *n, int depth) const;

protected:
	static void _bind_methods();
	void _notification(int what);

public:
	static JVDevTools *get() { return singleton_; }
	void setup(Role r, JVDevTools *host) { role_ = r; host_ = host; }
	void _ready() override;
	void _exit_tree() override;
	void _process(double delta) override;
	void _physics_process(double delta) override;
	void _input(const Ref<InputEvent> &e) override;
	void _on_pre_draw();
	void _on_post_draw();
	void _on_console_submit(const String &text);


	Dictionary profiler_start(double seconds, const String &tag);
	Dictionary profiler_stop();


	Dictionary profiler_toggle(double seconds, const String &tag);
	static Key key_from_name(const String &name);
	bool hotkeys_enabled() const { return hotkeys_; }
	void set_hotkeys(bool on);
	String profiler_menu_label() const;
	Dictionary profiler_status() const;
	Dictionary profiler_last(bool frames) const;
	Dictionary profiler_summary() const;
	Array profiler_worst(int64_t n) const;
	Dictionary profiler_hitches() const;
	bool profiler_capturing() const { return capturing_; }


	void debug_set_enabled(bool on);
	bool debug_enabled() const { return dbg_on_; }
	bool debug_set_log_level(const String &tag, int64_t level);
	Dictionary debug_log_levels() const;
	Dictionary debug_capabilities() const;
	Variant debug_tree(const String &path, int64_t depth) const;
	Dictionary debug_node(const String &path) const;
	Array debug_trace(int64_t n) const;
	void debug_trace_clear() { trace_.clear(); }
	String debug_dump();
	void trace(const String &kind, const String &name, const String &detail);
	void console_toggle();


	Dictionary consts_live() const;
	Dictionary consts_check_write(const String &source, const String &name, const Variant &value);
	Dictionary consts_audit();
	Dictionary consts_status() const;
	Array consts_findings() const;
	void consts_clear() { findings_.clear(); warned_.clear(); }
	bool consts_set_host_values(const Dictionary &values);
	void consts_tick(double dt);
};

}
