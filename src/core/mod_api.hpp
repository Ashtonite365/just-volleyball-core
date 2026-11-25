#pragma once
#include "../api/api_core.hpp"
#include "../common/prop_overrides.hpp"
#include "../gate/gameplay_settings.hpp"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <cstdint>
#include <map>
#include <vector>
#include <set>

namespace godot {

class JVModGate;








class JVModAPI : public Node {
	GDCLASS(JVModAPI, Node)

	jvm::api::Registry reg_;
	bool strict_ = false;
	bool strict_loaded_ = false;
	bool closing_ = false;
	std::map<String, bool> last_open_;
	std::set<String> logged_refusals_;
	uint64_t panel_id_ = 0;

	struct Sub { String mod; Callable fn; };
	std::map<String, std::vector<Sub>> subs_;
	double poll_t_ = 0.0;
	String last_mode_, last_stage_, last_ball_, last_map_, last_model_, last_scene_name_;
	uint64_t last_scene_id_ = 0;
	int last_menu_perf_ = -1;
	uint64_t goofy_relay_id_ = 0;
	bool events_primed_ = false;
	Node *lib(const char *which) const;
	Variant fwd(const String &mod, const char *cap, const char *which, const char *method, const Array &args = Array(), const Variant &dflt = Variant(), bool need_open = true);
	void emit_event(const String &name, const Dictionary &data);
	void poll_events();
	void hook_goofy_relay();
	String state_path(const String &mod) const;
	static void bind_core_methods();

	std::map<String, Ref<Resource>> phys_;
	std::map<String, jvm::PropOverrides> overrides_;

	jvm::gs::Registry gs_;
	struct GsHooks { Callable on_change, on_read; };
	std::map<String, GsHooks> gs_hooks_;
	std::map<String, Callable> gs_mod_change_;
	std::map<String, int64_t> gs_versions_;
	std::map<String, Variant> gs_effective_;
	bool gs_in_read_ = false, gs_dirty_ = false;
	double gs_t_ = 0.0;
	static void bind_gameplay_settings_methods();
	void gs_attach_ui();
	void gs_tick(double delta);
	void gs_update_role();
	void gs_refresh();
	String gs_state_path(const String &mod) const;
	bool gs_save(const String &mod);
	Dictionary gs_load(const String &mod, int64_t version, const Callable &migrate);
	Variant gs_read(const String &id);

	std::map<String, String> gs_mod_versions_;
	std::map<String, String> gs_peer_names_;
	bool gs_lobby_ = false, gs_loader_checked_ = false;
	double gs_lobby_t_ = 1.0;
	jvm::gs::Manifest gs_build_manifest() const;
	bool gs_loader_ok() const;
	void gs_lobby_tick(double delta);
	void gs_notice(const String &text);
	Dictionary gs_missing_of(const jvm::gs::Manifest &theirs);
	Dictionary gs_disabled_dict();
public:
	bool gs_match_veto(const String &mod, const char *cap) const;
private:

	JVModGate *gate() const;
	const jvm::api::GateView &view() const;
	jvm::api::GateView view_uncached() const;
	mutable jvm::api::GateView view_cache_;
	mutable uint64_t view_pf_ = UINT64_MAX, view_ph_ = UINT64_MAX;
	mutable bool view_dirty_ = true;
	std::map<String, bool> rpc_cache_;
	std::map<String, String> reg_log_;
	bool use(const String &mod, const char *cap, bool need_open = true);


	std::vector<ObjectID> balls_;
	bool balls_seeded_ = false;
	void _on_node_added_ball(Node *n);
	Array tracked_balls();
	void emit_changes();
	void load_strict();

protected:
	static void _bind_methods();

public:


	Node *core_library(const char *which) const { return lib(which); }
	Variant core_forward(const String &mod, const char *cap, const char *which, const char *method, const Array &a) { return fwd(mod, cap, which, method, a, Variant(), false); }
	JVModAPI();
	void _ready() override;
	void _exit_tree() override;
	void _process(double delta) override;
	void _on_gate_changed(bool active, String reason) { view_dirty_ = true; emit_changes(); }
	static bool is_rpc(Object *o, const String &method);



	Variant io_get(const String &mod, const String &cap, Object *o, const StringName &prop);
	bool io_set(const String &mod, const String &cap, Object *o, const StringName &prop, const Variant &v, bool undo);
	Variant io_call(const String &mod, const String &cap, Object *o, const StringName &method, const Array &args, bool undo);
	int64_t io_connect(const String &mod, const String &cap, Object *o, const StringName &sig, const Callable &c, int64_t flags);
	bool io_disconnect(const String &mod, const String &cap, Object *o, const StringName &sig, const Callable &c);
	Node *io_node(const String &mod, const String &cap, Node *from, const String &path);
	Node *io_autoload(const String &mod, const String &cap, const String &name);
	Object *io_singleton(const String &mod, const String &cap, const String &name);
	Ref<Resource> io_resource(const String &mod, const String &cap, const String &path);


	String get_api_version() const;
	Array get_capabilities() const;
	bool has_capability(const String &id) const;
	bool can(const String &mod, const String &cap) const;
	String why_not(const String &mod, const String &cap) const;
	bool is_open(const String &access) const;
	String get_reason(const String &access) const;
	String get_mode() const;
	Dictionary get_status() const;
	Dictionary get_coverage() const;
	Dictionary register_mod(const String &name, const String &version, const PackedStringArray &caps);
	Dictionary get_mods() const;
	bool library_allowed(const String &short_name);
	bool is_strict() { load_strict(); return strict_; }

	Dictionary load_config(const String &mod, const String &category);

	Node *get_current_scene(const String &mod);
	Array find_balls(const String &mod);
	Array find_players(const String &mod);
	bool is_ball(const String &mod, Object *o);
	Node *get_local_player(const String &mod);
	Object *get_rally_for(const String &mod, Node *player);

	Dictionary get_net_state(const String &mod);
	String lobby_data_get(const String &mod, const String &key);
	bool lobby_data_set(const String &mod, const String &key, const String &value);
	bool lobby_data_clear(const String &mod, const String &key);

	String ball_physics_open(const String &mod, const String &path);
	void ball_physics_close(const String &mod);
	Variant ball_physics_get(const String &mod, const String &prop);
	bool ball_physics_set(const String &mod, const String &prop, const Variant &value);
	bool ball_physics_restore(const String &mod, const String &prop, const Variant &value);
	Node *ball_get_model(const String &mod, Node *ball);
	Dictionary ball_get_flight(const String &mod, Node *ball);
	bool ball_set_flight(const String &mod, Node *ball, const Dictionary &d);
	bool connect_ball_struck(const String &mod, Node *ball, const Callable &c, int64_t flags);

	Vector3 node_get_scale(const String &mod, const String &cap, Node *n);
	bool node_set_scale(const String &mod, const String &cap, Node *n, const Vector3 &s);
	bool node_restore_scale(const String &mod, const String &cap, Node *n, const Vector3 &s);

	bool connect_player_touched(const String &mod, Node *player, const Callable &c);

	Dictionary rally_touch_state(const String &mod, Object *rally);
	bool rally_set_touch_count(const String &mod, Object *rally, int64_t count);
	bool rally_clear_last_toucher(const String &mod, Object *rally);
	Node *modifiers_panel_find(const String &mod);
	Variant modifiers_panel_rows(const String &mod, Node *panel);
	Dictionary modifiers_panel_add_rows(const String &mod, Node *panel, const Array &defs, const Callable &on_changed);
	void modifiers_panel_show_rows(const String &mod, const Array &rows, bool show);
	void modifiers_panel_set_values(const String &mod, Node *panel, const Dictionary &values);

	double time_scale_get() const;
	bool time_scale_set(const String &mod, double v);
	bool time_scale_restore(const String &mod, double v);
	void overrides_set_rules(const String &mod, const Array &rules);
	int64_t overrides_apply(const String &mod);
	void overrides_revert(const String &mod);

	Variant game_get(const String &mod, Object *o, const String &prop);
	bool game_set(const String &mod, Object *o, const String &prop, const Variant &v);
	Variant game_call(const String &mod, Object *o, const String &method, const Array &args);
	bool game_connect(const String &mod, Object *o, const String &signal, const Callable &c);




	Dictionary gate_matrix() const;
	Dictionary net_safety(const String &cap) const;
	Dictionary libraries_status(const String &mod);
	Dictionary library_status(const String &mod, const String &library);
	bool library_reload(const String &mod, const String &library);
	Variant state_get(const String &mod, const String &key, const Variant &dflt);
	bool state_set(const String &mod, const String &key, const Variant &value);
	bool state_erase(const String &mod, const String &key);
	PackedStringArray state_keys(const String &mod);
	Dictionary config_validate(const String &mod, const String &library, const Dictionary &config);

	Array events_list() const;
	bool subscribe(const String &mod, const String &event, const Callable &fn);
	bool unsubscribe(const String &mod, const String &event, const Callable &fn);
	void _on_goofy_params_changed(const Array &changed, const Dictionary &effective);
	void _on_goofy_ball_mode_changed(const String &mode, bool force);
	void _on_goofy_physics_allowed_changed(bool allowed);
	void _on_goofy_game_event(const String &kind, const String &name, int64_t index, const Array &args);
	void _on_goofy_sync_changed(const String &role, int64_t host_values);

	bool connect_ball_event(const String &mod, Node *ball, const String &name, const Callable &fn);
	bool connect_player_event(const String &mod, Node *player, const String &name, const Callable &fn);
	bool connect_net_signal(const String &mod, const String &name, const Callable &fn);
	Dictionary stage_get_nodes(const String &mod);
	Dictionary player_get_state(const String &mod, Node *player);
	Variant player_feel_get(const String &mod, const String &prop);

	Array packs_list(const String &mod, const String &type);

	Dictionary content_status(const String &mod);
	bool content_fetch(const String &mod, const String &id);
	Dictionary pack_info(const String &mod, const String &id);
	bool packs_rescan(const String &mod);
	String stage_get_picked(const String &mod);
	bool stage_set_picked(const String &mod, const String &id);
	String ball_get_picked(const String &mod);
	bool ball_set_picked(const String &mod, const String &id);
	bool cosmetics_apply(const String &mod);
	Dictionary textures_status(const String &mod);
	Dictionary textures_sync_status(const String &mod);
	Dictionary textures_cache_info(const String &mod);
	Variant texture_get_cached(const String &mod, const String &path);
	Dictionary court_env_status(const String &mod);
	Dictionary court_sky_status(const String &mod);
	Dictionary court_env_info(const String &mod, const String &stage_id);
	bool court_env_load(const String &mod, const String &stage_id);
	Dictionary accessories_status(const String &mod);
	Array accessories_list(const String &mod, const String &category);

	Array models_list(const String &mod);
	String model_get_saved(const String &mod);
	String model_get_shown(const String &mod);
	bool model_is_previewing(const String &mod);
	Dictionary model_settings_get(const String &mod, const String &id);
	Dictionary models_status(const String &mod);
	Variant models_rig_report(const String &mod);
	bool model_preview(const String &mod, const String &id);
	bool model_apply(const String &mod, const String &id);
	bool model_end_preview(const String &mod);
	bool model_set_color(const String &mod, const String &id, const Color &c);
	bool model_reset_color(const String &mod, const String &id);
	bool model_set_option(const String &mod, const String &id, const String &key, const Variant &value);

	Dictionary loader_status(const String &mod);
	Dictionary loader_bridge_status(const String &mod);
	bool loader_is_map(const String &mod, const String &stage_id);
	String loader_active_map(const String &mod);
	Dictionary loader_map_goofy_block(const String &mod, const String &stage_id);
	bool loader_refresh(const String &mod);

	bool goofy_available(const String &mod);
	String goofy_api_version(const String &mod);
	Array goofy_schema(const String &mod);
	Dictionary goofy_param_info(const String &mod, const String &id);
	Variant goofy_param_get(const String &mod, const String &id);
	Dictionary goofy_params(const String &mod);
	Variant goofy_stock(const String &mod, const String &id);
	Array goofy_layers(const String &mod);
	String goofy_ball_mode_get(const String &mod);
	String goofy_loader_map(const String &mod);
	bool goofy_physics_allowed(const String &mod);
	Dictionary goofy_status(const String &mod);


	Dictionary player_consts_get(const String &mod);
	Dictionary player_consts_set(const String &mod, const String &name, double value);
	Dictionary player_consts_reset(const String &mod, const String &name);
	bool player_consts_locked(const String &mod);
	Dictionary goofy_set_override(const String &mod, const Dictionary &params, int64_t priority, const String &scope);
	bool goofy_clear_override(const String &mod);
	bool goofy_ball_mode_set(const String &mod, const String &mode);
	Dictionary goofy_bridge_info(const String &mod);
	bool goofy_loader_map_set(const String &mod, const String &map);
	Variant goofy_pack_apply(const String &mod, const String &map, const Dictionary &params, int64_t version);
	bool goofy_pack_clear(const String &mod, const String &map);
	Dictionary goofy_rules_get(const String &mod);
	bool goofy_rules_set(const String &mod, int64_t max_hits, bool no_double_hit);

	Array goofy_ball_schema(const String &mod);
	Array goofy_player_schema(const String &mod);
	Array goofy_balls(const String &mod);
	Dictionary goofy_ball_state(const String &mod, int64_t index);
	bool goofy_ball_set_velocity(const String &mod, int64_t index, const Vector3 &velocity);
	bool goofy_ball_set_curve(const String &mod, int64_t index, double curve);
	bool goofy_ball_impulse(const String &mod, int64_t index, const Vector3 &delta_v);
	Array goofy_players(const String &mod);
	Dictionary goofy_player_state(const String &mod, int64_t index);
	Dictionary goofy_player_cosmetics(const String &mod, int64_t index);
	Dictionary goofy_player_set_override(const String &mod, const Dictionary &params, int64_t priority, const String &scope);
	bool goofy_player_clear_override(const String &mod);
	Dictionary goofy_sync_status(const String &mod);

	PackedStringArray shots_list(const String &mod);
	String shots_armed(const String &mod);
	Variant shots_last(const String &mod, const String &kind);
	Variant shots_spin_bar(const String &mod);
	Variant shots_range_box(const String &mod);
	Dictionary shots_counts(const String &mod);
	Dictionary shots_status(const String &mod);
	bool shot_arm(const String &mod, const String &id);
	bool shots_debug_set(const String &mod, bool on);
	bool shots_debug_visible(const String &mod);
	String shots_debug_text(const String &mod);

	bool picker_ball_open(const String &mod);
	bool picker_stage_inject(const String &mod);
	bool mods_panel_open(const String &mod);
	bool mods_panel_close(const String &mod);
	bool mods_panel_show_list(const String &mod);
	bool mods_panel_show_detail(const String &mod, const String &mod_id);
	Dictionary mods_panel_info(const String &mod);
	Variant mods_config_files(const String &mod, const String &mod_id);
	Variant mods_config_save(const String &mod, const String &path, const String &text, bool live);
	bool menu_perf_get(const String &mod);
	bool menu_perf_set(const String &mod, bool on);
	Dictionary menu_perf_info(const String &mod);
	bool menu_perf_reset(const String &mod);

	static void bind_dev_methods();
	Dictionary profiler_start(const String &mod, double seconds, const String &tag);
	Dictionary profiler_stop(const String &mod);
	Dictionary profiler_toggle(const String &mod, double seconds, const String &tag);
	Dictionary profiler_status(const String &mod);
	Dictionary profiler_last_capture(const String &mod, bool frames);
	Dictionary profiler_summary(const String &mod);
	Array profiler_worst_frames(const String &mod, int64_t n);
	Dictionary profiler_hitches(const String &mod);
	bool debug_set_enabled(const String &mod, bool on);
	bool debug_is_enabled(const String &mod);
	bool debug_set_log_level(const String &mod, const String &tag, int64_t level);
	Dictionary debug_log_levels(const String &mod);
	Dictionary debug_capabilities(const String &mod);
	Variant debug_tree(const String &mod, const String &path, int64_t depth);
	Dictionary debug_node(const String &mod, const String &path);
	Array debug_trace(const String &mod, int64_t n);
	bool debug_trace_clear(const String &mod);
	String debug_dump(const String &mod);
	bool debug_console_toggle(const String &mod);
	Dictionary consts_live(const String &mod);
	Dictionary consts_status(const String &mod);
	Dictionary consts_check_write(const String &mod, const String &name, const Variant &value);
	Dictionary consts_audit(const String &mod);
	Array consts_findings(const String &mod);
	bool consts_clear_findings(const String &mod);
	bool consts_set_host_values(const String &mod, const Dictionary &values);

	Dictionary gameplay_register(const String &mod, const Array &defs, const Dictionary &options);
	Dictionary gameplay_register_setting(const String &mod, const Dictionary &def);
	int64_t gameplay_unregister(const String &mod);
	Variant gameplay_get(const String &mod, const String &id);
	Dictionary gameplay_get_all(const String &mod, const String &owner);
	Dictionary gameplay_set(const String &mod, const String &id, const Variant &value);
	bool gameplay_reset(const String &mod, const String &id);
	Array gameplay_list(const String &mod);
	Dictionary gameplay_info(const String &mod, const String &id);
	String gameplay_role(const String &mod);
	Dictionary gameplay_snapshot(const String &mod);
	PackedByteArray gameplay_snapshot_bytes(const String &mod);
	Dictionary gameplay_apply_snapshot(const String &mod, const Variant &payload);
	bool gameplay_clear_host(const String &mod);

	Dictionary gameplay_manifest(const String &mod);
	PackedByteArray gameplay_manifest_bytes(const String &mod);
	Dictionary gameplay_set_peer_manifest(const String &mod, const String &player_id, const Variant &payload, const String &player_name);
	bool gameplay_remove_peer(const String &mod, const String &player_id);
	Dictionary gameplay_set_host_manifest(const String &mod, const Variant &payload);
	bool gameplay_mod_active_for(const String &mod, const String &mod_id, const String &player_id);
	Dictionary gameplay_player_mods(const String &mod);
	Dictionary gameplay_match_start(const String &mod);
	bool gameplay_match_end(const String &mod);
	Dictionary gameplay_match_status(const String &mod);

	Array gs_ui_list();
	Dictionary gs_ui_set(const String &id, const Variant &value);
	bool gs_ui_reset(const String &id);
	String gs_ui_role() const { return gs_.role() == jvm::gs::Role::Local && gs_.match_locked() ? String("match") : String(jvm::gs::to_string(gs_.role())); }
	uint64_t gs_ui_revision() const { return gs_.revision(); }
	uint64_t gs_ui_host_hash() const;

	Dictionary game_rules_info(const String &mod);
	Dictionary game_rules_state(const String &mod, Object *rally);

	Array script_mods_list() const;
	void load_script_mods();
};

}
