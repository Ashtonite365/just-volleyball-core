



#pragma once
#include "jvm_common.hpp"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

namespace jvm {

struct PropOverrides {
	struct Saved { uint64_t id; godot::NodePath prop; godot::Variant original; };
	std::vector<Saved> saved;
	godot::Array rules;

	bool already(uint64_t id, const godot::NodePath &p) const {
		for (auto &s : saved) if (s.id == id && s.prop == p) return true;
		return false;
	}


	int apply(godot::Node *from, const godot::String &tag) {
		int n = 0;
		for (int r = 0; r < rules.size(); r++) {
			if (rules[r].get_type() != godot::Variant::DICTIONARY) continue;
			godot::Dictionary rule = rules[r];
			godot::TypedArray<godot::Node> nodes = find_targets(from, rule.get("target", godot::Variant()));
			godot::Dictionary set = rule.get("set", godot::Dictionary());
			godot::Array keys = set.keys();
			for (int i = 0; i < nodes.size(); i++) {
				godot::Node *node = godot::Object::cast_to<godot::Node>(nodes[i]);
				if (!node) continue;
				for (int k = 0; k < keys.size(); k++) {
					godot::NodePath p(godot::String(keys[k]));
					if (already(node->get_instance_id(), p)) continue;
					bool valid = false;
					godot::Variant orig = node->get_indexed(p);
					valid = orig.get_type() != godot::Variant::NIL;
					if (!valid) { warn(tag, godot::String(node->get_path()) + " has no property " + godot::String(keys[k])); continue; }
					saved.push_back({ node->get_instance_id(), p, orig });
					node->set_indexed(p, set[keys[k]]);
					n++;
				}
			}
		}
		return n;
	}
	void revert() {
		for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
			godot::Object *o = godot::ObjectDB::get_instance(it->id);
			if (o) o->set_indexed(it->prop, it->original);
		}
		saved.clear();
	}
};

}
