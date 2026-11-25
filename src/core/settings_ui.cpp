
#include "settings_ui.hpp"
#include "mod_api.hpp"
#include "../common/api_client.hpp"
#include "../common/jvm_common.hpp"

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/range.hpp>
#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/tab_container.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <cmath>
#include <map>
#include <vector>

using namespace godot;

static const char *kTab = "mods";
static jvm::GameIO &IO() {
	static jvm::GameIO io = [] {
		jvm::GameIO g;
		g.mod = "core_settings_ui";
		g.api_version = "1.3";
		g.writes_need_open = false;
		g.caps.push_back("gameplay.settings");
		g.caps.push_back("gate.mode");
		return g;
	}();
	return io;
}
static Object *obj(const Variant &v) { return v.get_type() == Variant::OBJECT ? v.operator Object *() : nullptr; }

void JVSettingsUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_node_added", "node"), &JVSettingsUI::_on_node_added);
	ClassDB::bind_method(D_METHOD("_on_bool", "on", "id"), &JVSettingsUI::_on_bool);
	ClassDB::bind_method(D_METHOD("_on_num", "v", "id"), &JVSettingsUI::_on_num);
	ClassDB::bind_method(D_METHOD("_on_enum", "i", "id"), &JVSettingsUI::_on_enum);
	ClassDB::bind_method(D_METHOD("_on_vec", "v", "id", "comp"), &JVSettingsUI::_on_vec);
	ClassDB::bind_method(D_METHOD("_fmt", "v", "id"), &JVSettingsUI::_fmt);
	ClassDB::bind_method(D_METHOD("get_info"), &JVSettingsUI::get_info);
	ClassDB::bind_method(D_METHOD("refresh_now"), &JVSettingsUI::refresh_now);
}

JVModAPI *JVSettingsUI::api() const { return Object::cast_to<JVModAPI>(get_parent()); }

void JVSettingsUI::_ready() {
	set_process(true);

	get_tree()->connect("node_added", Callable(this, "_on_node_added"));
	scan(get_tree()->get_root(), 0);
}

void JVSettingsUI::scan(Node *n, int depth) {
	if (!n || depth > 24) return;
	_on_node_added(n);
	for (int i = 0; i < n->get_child_count(); i++) scan(n->get_child(i), depth + 1);
}

void JVSettingsUI::_on_node_added(Node *n) {

	if (!Object::cast_to<VBoxContainer>(n) || !jvm::script_is(n, "SettingsPanel")) return;
	panels_.insert(n->get_instance_id());
}

String JVSettingsUI::signature() const {
	JVModAPI *a = api();
	if (!a) return "";
	String role = a->gs_ui_role();
	return String::num_uint64(a->gs_ui_revision()) + "|" + role + "|" + (role == "client" ? String::num_uint64(a->gs_ui_host_hash()) : String());
}

void JVSettingsUI::_process(double delta) {
	if (notice_id_ && (notice_t_ -= delta) <= 0.0) {
		if (Node *n = Object::cast_to<Node>(ObjectDB::get_instance(notice_id_))) n->queue_free();
		notice_id_ = 0;
	}
	t_ += delta;
	if (t_ < 0.2 || panels_.empty()) return;
	t_ = 0.0;
	for (auto it = panels_.begin(); it != panels_.end();) {
		Node *p = Object::cast_to<Node>(ObjectDB::get_instance(*it));
		if (!p) { it = panels_.erase(it); continue; }
		++it;
		if (p->is_inside_tree()) ensure(p);
	}
}

int64_t JVSettingsUI::refresh_now() {
	int64_t n = 0;
	for (uint64_t id : panels_) {
		Node *p = Object::cast_to<Node>(ObjectDB::get_instance(id));
		if (!p || !p->is_inside_tree()) continue;
		TabContainer *tabs = Object::cast_to<TabContainer>(obj(IO().get("gameplay.settings", p, "_tabs")));
		if (tabs) if (Node *t = tabs->get_node_or_null(NodePath(kTab))) t->set_meta("jvm_gs_sig", "");
		ensure(p);
		n++;
	}
	return n;
}

void JVSettingsUI::ensure(Node *panel) {
	JVModAPI *a = api();
	if (!a) return;

	TabContainer *tabs = Object::cast_to<TabContainer>(obj(IO().get("gameplay.settings", panel, "_tabs")));
	if (!tabs || !tabs->is_inside_tree()) return;
	Array list = a->gs_ui_list();
	Node *have = tabs->get_node_or_null(NodePath(kTab));
	if (have && !have->has_meta("jvm_gs")) return;
	if (list.is_empty()) {
		if (have) { tabs->remove_child(have); have->queue_free(); }
		return;
	}
	String sig = signature();
	if (have && String(have->get_meta("jvm_gs_sig", "")) == sig) return;
	VBoxContainer *col = nullptr;
	if (have) col = Object::cast_to<VBoxContainer>(Object::cast_to<Node>(ObjectDB::get_instance(uint64_t(int64_t(have->get_meta("jvm_gs_col", int64_t(0)))))));
	if (!col) {
		if (have) { tabs->remove_child(have); have->queue_free(); have = nullptr; }
		const int keep = tabs->get_current_tab();
		if (panel->has_method("_tab")) col = Object::cast_to<VBoxContainer>(obj(IO().call("gameplay.settings", panel, "_tab", tabs, String(kTab))));
		if (!col) {
			ScrollContainer *scroll = memnew(ScrollContainer);
			scroll->set_name(kTab);
			scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
			MarginContainer *pad = memnew(MarginContainer);
			for (const char *side : { "margin_left", "margin_right", "margin_top", "margin_bottom" }) pad->add_theme_constant_override(side, 14);
			pad->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			col = memnew(VBoxContainer);
			col->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			col->add_theme_constant_override("separation", 12);
			pad->add_child(col); scroll->add_child(pad); tabs->add_child(scroll);
		}
		if (keep >= 0 && keep < tabs->get_tab_count()) tabs->set_current_tab(keep);
		Node *scroll = col->get_parent() ? col->get_parent()->get_parent() : nullptr;
		if (!scroll || String(scroll->get_name()) != kTab) return;
		scroll->set_meta("jvm_gs", true);
		scroll->set_meta("jvm_gs_col", int64_t(col->get_instance_id()));
		have = scroll;
	}
	build(panel, col, list, a->gs_ui_role());
	have->set_meta("jvm_gs_sig", sig);
}

void JVSettingsUI::lock_subtree(Node *n, bool locked) {
	if (!n) return;
	if (BaseButton *b = Object::cast_to<BaseButton>(n)) b->set_disabled(locked);
	if (Range *r = Object::cast_to<Range>(n)) if (HSlider *s = Object::cast_to<HSlider>(r)) s->set_editable(!locked);
	if (Control *c = Object::cast_to<Control>(n)) if (locked) c->set_focus_mode(Control::FOCUS_NONE);
	for (int i = 0; i < n->get_child_count(); i++) lock_subtree(n->get_child(i), locked);
}

static void tip_subtree(Node *n, const String &tip) {
	if (Control *c = Object::cast_to<Control>(n)) c->set_tooltip_text(tip);
	for (int i = 0; i < n->get_child_count(); i++) tip_subtree(n->get_child(i), tip);
}

static Label *plain_label(Control *parent, const String &text) {
	Label *l = memnew(Label);
	l->set_text(text);
	l->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	l->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	parent->add_child(l);
	return l;
}

void JVSettingsUI::build(Node *panel, VBoxContainer *col, const Array &list, const String &role) {
	for (int i = col->get_child_count() - 1; i >= 0; i--) { Node *c = col->get_child(i); col->remove_child(c); c->queue_free(); }
	const bool locked = role == "client" || role == "match";
	const bool builders = panel->has_method("_check") && panel->has_method("_range") && panel->has_method("_select") && panel->has_method("_subheader");
	auto subheader = [&](const String &t) {
		if (builders) IO().call("gameplay.settings", panel, "_subheader", col, t);
		else { Label *l = plain_label(col, t); l->add_theme_font_size_override("font_size", 17); }
	};
	subheader("gameplay");
	String note = role == "client" ? String("the host controls these in this lobby - showing the host's values")
	             : role == "match" ? String("locked for this match (decided at match start); a mod someone in the lobby does not have is off for everyone")
	             : role == "off" ? String("not used in public lobbies / quick match: everyone plays stock")
	                             : String("saved per mod. offline they apply live; as host of a LAN / private / friends lobby everyone uses yours");
	Label *info = plain_label(col, note);
	info->set_name("JVGameplayNote");
	info->set_modulate(Color(1, 1, 1, 0.75));

	std::vector<String> cats;
	std::map<String, std::vector<Dictionary>> by_cat;
	for (int i = 0; i < list.size(); i++) {
		Dictionary s = list[i];
		if (!s.has("type")) continue;
		String c = s.get("category", "mods");
		if (!by_cat.count(c)) cats.push_back(c);
		by_cat[c].push_back(s);
	}
	for (auto &c : cats) {
		subheader(c);
		for (Dictionary &s : by_cat[c]) {
			const String id = s["id"], type = s["type"], label = s["label"], tip = s.get("tooltip", "");
			const Variant value = s["value"];
			const Variant shown = role == "off" ? s["local"] : value;
			const int before = col->get_child_count();
			if (type == "bool") {
				if (builders) IO().call("gameplay.settings", panel, "_check", col, label, tip, bool(shown), Callable(this, "_on_bool").bind(id), String());
				else { CheckBox *cb = memnew(CheckBox); cb->set_text(label); cb->set_pressed(bool(shown)); cb->connect("toggled", Callable(this, "_on_bool").bind(id)); col->add_child(cb); }
			} else if (type == "enum") {
				PackedStringArray o = s["options"];
				Array opts; int cur = 0, def = 0;
				for (int i = 0; i < o.size(); i++) { opts.push_back(o[i]); if (o[i] == String(shown)) cur = i; if (o[i] == String(s["default"])) def = i; }
				if (builders) IO().call("gameplay.settings", panel, "_select", col, label, opts, cur, Callable(this, "_on_enum").bind(id), def);
				else {
					HBoxContainer *row = memnew(HBoxContainer); Label *l = memnew(Label); l->set_text(label); row->add_child(l);
					OptionButton *ob = memnew(OptionButton); for (int i = 0; i < o.size(); i++) ob->add_item(o[i]);
					ob->select(cur); ob->connect("item_selected", Callable(this, "_on_enum").bind(id)); row->add_child(ob); col->add_child(row);
				}
			} else {
				const double mn = s["min"], mx = s["max"];
				double step = s["step"];
				if (step <= 0) step = type == "int" ? 1.0 : (mx - mn) / 100.0;
				std::vector<double> comps; std::vector<double> defs;
				const Variant dv = s["default"];
				if (type == "vec") {
					int dim = s["dim"];
					for (int k = 0; k < dim; k++) {
						comps.push_back(dim == 2 ? double(Vector2(shown)[k]) : dim == 3 ? double(Vector3(shown)[k]) : double(Vector4(shown)[k]));
						defs.push_back(dim == 2 ? double(Vector2(dv)[k]) : dim == 3 ? double(Vector3(dv)[k]) : double(Vector4(dv)[k]));
					}
				} else { comps.push_back(double(shown)); defs.push_back(double(dv)); }
				static const char *axis[] = { " x", " y", " z", " w" };
				for (size_t k = 0; k < comps.size(); k++) {
					String lab = comps.size() > 1 ? label + String(axis[k]) : label;
					Callable apply = type == "vec" ? Callable(this, "_on_vec").bind(id, int64_t(k)) : Callable(this, "_on_num").bind(id);
					if (builders) IO().call("gameplay.settings", panel, "_range", col, lab, mn, mx, step, comps[k], Callable(this, "_fmt").bind(id), apply, defs[k]);
					else {
						HBoxContainer *row = memnew(HBoxContainer); Label *l = memnew(Label); l->set_text(lab); row->add_child(l);
						HSlider *sl = memnew(HSlider); sl->set_min(mn); sl->set_max(mx); sl->set_step(step); sl->set_value(comps[k]);
						sl->set_h_size_flags(Control::SIZE_EXPAND_FILL); sl->connect("value_changed", apply); row->add_child(sl); col->add_child(row);
					}
				}
			}
			for (int i = before; i < col->get_child_count(); i++) {
				Node *row = col->get_child(i);
				row->set_meta("jvm_gs_id", id);
				if (!tip.is_empty()) tip_subtree(row, tip);
				if (locked) { lock_subtree(row, true); if (Control *rc = Object::cast_to<Control>(row)) rc->set_modulate(Color(1, 1, 1, 0.5)); }
			}
		}
	}
	builds_++;
}

void JVSettingsUI::_on_bool(bool on, const String &id) { if (JVModAPI *a = api()) a->gs_ui_set(id, on); }
void JVSettingsUI::_on_num(double v, const String &id) { if (JVModAPI *a = api()) a->gs_ui_set(id, v); }
void JVSettingsUI::_on_enum(int64_t i, const String &id) { if (JVModAPI *a = api()) a->gs_ui_set(id, i); }
void JVSettingsUI::_on_vec(double v, const String &id, int64_t comp) {
	JVModAPI *a = api();
	if (!a) return;
	Array list = a->gs_ui_list();
	for (int i = 0; i < list.size(); i++) {
		Dictionary s = list[i];
		if (String(s.get("id", "")) != id) continue;
		Variant cur = s["local"];
		Array out;
		int dim = s["dim"];
		for (int k = 0; k < dim; k++) out.push_back(k == comp ? v : (dim == 2 ? double(Vector2(cur)[k]) : dim == 3 ? double(Vector3(cur)[k]) : double(Vector4(cur)[k])));
		a->gs_ui_set(id, out);
		return;
	}
}

String JVSettingsUI::_fmt(double v, const String &id) {
	JVModAPI *a = api();
	if (a) {
		Array list = a->gs_ui_list();
		for (int i = 0; i < list.size(); i++) {
			Dictionary s = list[i];
			if (String(s.get("id", "")) != id) continue;
			if (String(s["type"]) == "int") return String::num_int64(int64_t(std::llround(v)));
			double step = s["step"];
			int dec = step > 0 && step < 1 ? int(std::ceil(-std::log10(step) - 1e-9)) : 2;
			return String::num(v, dec < 0 ? 0 : (dec > 4 ? 4 : dec));
		}
	}
	return String::num(v, 2);
}

Dictionary JVSettingsUI::get_info() const {
	Dictionary d;
	d["panels"] = int64_t(panels_.size());
	d["builds"] = builds_;
	d["signature"] = signature();
	d["notices"] = notices_;
	d["notice_visible"] = notice_id_ != 0;
	return d;
}


void JVSettingsUI::show_notice(const String &text) {
	notices_.push_back(text);
	if (notice_id_) if (Node *n = Object::cast_to<Node>(ObjectDB::get_instance(notice_id_))) n->queue_free();
	CanvasLayer *layer = memnew(CanvasLayer);
	layer->set_name("JVGameplayNotice");
	layer->set_layer(120);
	PanelContainer *box = memnew(PanelContainer);
	box->set_anchors_preset(Control::PRESET_CENTER_TOP);
	box->set_position(Vector2(0, 24));
	box->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
	box->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	Label *l = memnew(Label);
	l->set_text(text);
	l->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	l->set_custom_minimum_size(Vector2(520, 0));
	l->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	l->add_theme_color_override("font_color", Color(1.0, 0.85, 0.4));
	box->add_child(l);
	layer->add_child(box);
	add_child(layer);
	notice_id_ = layer->get_instance_id();
	notice_t_ = 10.0;
}
