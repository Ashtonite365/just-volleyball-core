#pragma once
#include "../gate/gate_logic.hpp"

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {




class JVModGate : public Node {
	GDCLASS(JVModGate, Node)

	jvm::gate::GateStateMachine sm_{ 0.5 };
	jvm::gate::GateStateMachine sm_cos_{ 0.5 };
	jvm::gate::Policy policy_;
	Array probes_;
	bool kill_switch_ = false;
	Array last_obs_;
	String mode_ = "unknown";
	int64_t full_changed_frame_ = -1, cos_changed_frame_ = -1, mode_changed_frame_ = -1;
	String last_logged_reason_;

	bool pin_checked_ = false;
	bool pin_ok_ = false;
	Dictionary pin_;

	Dictionary libs_;

	String last_sig_;
	double last_write_ = -1e9;
	int write_fail_ = 0;
	int status_followup_ = 0;
	bool status_content_dirty_ = true;
	Dictionary last_built_status_;
	double last_status_build_ms_ = 0;
	bool last_status_cached_ = false;

	CanvasLayer *ui_ = nullptr;
	Label *overlay_ = nullptr;
	Label *banner_ = nullptr;
	double banner_t_ = 0.0;
	bool overlay_on_ = false;

	double now_s() const;
	bool evaluate();
	std::vector<jvm::gate::Observation> run_probes(double now);
	bool probe_node_property(const Dictionary &spec, jvm::gate::Observation &o);
	bool probe_steam_lobby_data(const Dictionary &spec, jvm::gate::Observation &o);
	bool probe_multiplayer_peer(const Dictionary &spec, jvm::gate::Observation &o);
	bool probe_test(const Dictionary &spec, jvm::gate::Observation &o);
	bool probe_jv_net(const Dictionary &spec, jvm::gate::Observation &o);
	void apply_role(const Dictionary &spec, const String &raw, jvm::gate::Observation &o);
	void ensure_ui();
	void update_status(bool force);
	bool write_status_atomic(const String &json);
	static void stamp_volatile_status(Dictionary &st);

protected:
	static void _bind_methods();

public:
	JVModGate();
	void _ready() override;
	void _process(double delta) override;
	void _input(const Ref<InputEvent> &event) override;

	bool is_active() const { return pin_ok_ && sm_.active(); }
	bool is_cosmetic_active() const { return pin_ok_ && sm_cos_.active(); }
	String get_reason() const;
	String get_cosmetic_reason() const;
	String get_mode() const { return mode_; }
	Array get_observations() const { return last_obs_; }
	bool verify_version();
	bool pin_ok() const { return pin_ok_; }
	Dictionary get_pin_result() const { return pin_; }
	void register_library(const String &name, const String &state, const String &reason);

	bool api_library_allowed(const String &short_name);
	Dictionary build_status();
	void mark_status_dirty() { status_content_dirty_ = true; }
	double get_last_status_build_ms() const { return last_status_build_ms_; }
	bool status_was_cached_build() const { return last_status_cached_; }


	bool status_write_through() const { return false; }
	void reload_config();
	void force_off(const String &why);
	void set_overlay_visible(bool on);
};

}
