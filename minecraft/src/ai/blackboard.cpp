#include "blackboard.h"

namespace godot {

void Blackboard::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_value", "key", "value"), &Blackboard::set_value);
	ClassDB::bind_method(D_METHOD("get_value", "key", "default"), &Blackboard::get_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_value", "key"), &Blackboard::has_value);
	ClassDB::bind_method(D_METHOD("erase_value", "key"), &Blackboard::erase_value);
	ClassDB::bind_method(D_METHOD("clear"), &Blackboard::clear);
}

Blackboard::Blackboard() {}
Blackboard::~Blackboard() {}

void Blackboard::set_value(const String &p_key, const Variant &p_value) {
	data[p_key] = p_value;
}

Variant Blackboard::get_value(const String &p_key, const Variant &p_default) const {
	return data.get(p_key, p_default);
}

bool Blackboard::has_value(const String &p_key) const {
	return data.has(p_key);
}

void Blackboard::erase_value(const String &p_key) {
	data.erase(p_key);
}

void Blackboard::clear() {
	data.clear();
}

} // namespace godot
