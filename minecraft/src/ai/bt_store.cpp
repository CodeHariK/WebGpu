#include "bt_store.h"

namespace godot {

void BTStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_value", "key", "value"), &BTStore::set_value);
	ClassDB::bind_method(D_METHOD("get_value", "key", "default"), &BTStore::get_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("has_value", "key"), &BTStore::has_value);
	ClassDB::bind_method(D_METHOD("erase_value", "key"), &BTStore::erase_value);
	ClassDB::bind_method(D_METHOD("clear"), &BTStore::clear);
}

BTStore::BTStore() {}
BTStore::~BTStore() {}

void BTStore::set_value(const String &p_key, const Variant &p_value) {
	data[p_key] = p_value;
}

Variant BTStore::get_value(const String &p_key, const Variant &p_default) const {
	return data.get(p_key, p_default);
}

bool BTStore::has_value(const String &p_key) const {
	return data.has(p_key);
}

void BTStore::erase_value(const String &p_key) {
	data.erase(p_key);
}

void BTStore::clear() {
	data.clear();
}

} // namespace godot
