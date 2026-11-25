#pragma once
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {







class JVMenuPerf : public Node {
	GDCLASS(JVMenuPerf, Node)
public:
	enum Mode { Host, Pre, Post };

private:
	Mode mode_ = Host;
	JVMenuPerf *host_ = nullptr;
	bool enabled_ = true;
	bool loaded_cfg_ = false;
	uint64_t menu_id_ = 0;
	Array saved_tilt_, saved_lean_;
	Variant saved_top_strip_, saved_bottom_strip_;
	bool stole_tilt_ = false, stole_strips_ = false;
	int last_top_h_ = -1, last_bottom_h_ = -1;


	int64_t frames_ = 0, tilt_runs_ = 0, z_skipped_ = 0, strip_skipped_ = 0, strip_writes_ = 0;

	void load_cfg();
	Node *menu() const;
	void pre_tick();
	void post_tick(double delta);
	void tilt_cards(Node *menu, double delta);
	void lean_tiles(Node *menu, double delta);

protected:
	static void _bind_methods();

public:
	void setup(Mode mode, JVMenuPerf *host);
	void _ready() override;
	void _process(double delta) override;
	void set_enabled(bool on);
	bool is_enabled() const { return host_ ? host_->enabled_ : enabled_; }
	Dictionary get_info() const;
	void reset_counters();
};

}
