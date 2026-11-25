#pragma once
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <set>

namespace godot {

class JVModAPI;









class JVSettingsUI : public Node {
	GDCLASS(JVSettingsUI, Node)
	std::set<uint64_t> panels_;
	double t_ = 0.0;
	int builds_ = 0;
	uint64_t notice_id_ = 0;
	double notice_t_ = 0.0;
	Array notices_;
	JVModAPI *api() const;
	void scan(Node *n, int depth);
	void ensure(Node *panel);
	void build(Node *panel, VBoxContainer *col, const Array &list, const String &role);
	String signature() const;
	static void lock_subtree(Node *n, bool locked);

protected:
	static void _bind_methods();

public:
	void _ready() override;
	void _process(double delta) override;
	void _on_node_added(Node *n);
	void _on_bool(bool on, const String &id);
	void _on_num(double v, const String &id);
	void _on_enum(int64_t i, const String &id);
	void _on_vec(double v, const String &id, int64_t comp);
	String _fmt(double v, const String &id);
	Dictionary get_info() const;
	int64_t refresh_now();
	void show_notice(const String &text);
};

}
