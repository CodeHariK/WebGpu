#include "inventory.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/json.hpp>

namespace godot {

void Inventory::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_item", "name", "quantity"), &Inventory::add_item);
	ClassDB::bind_method(D_METHOD("try_consume", "name", "quantity"), &Inventory::try_consume);
	ClassDB::bind_method(D_METHOD("get_item_quantity", "name"), &Inventory::get_item_quantity);
	ClassDB::bind_method(D_METHOD("has_item", "name", "min_quantity"), &Inventory::has_item, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("clear"), &Inventory::clear);
	
	ClassDB::bind_method(D_METHOD("set_items", "items"), &Inventory::set_items);
	ClassDB::bind_method(D_METHOD("get_items"), &Inventory::get_items);
	
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "items"), "set_items", "get_items");
}

Inventory::Inventory() {}
Inventory::~Inventory() {}

void Inventory::add_item(const String &p_name, int p_quantity) {
	int current = items.get(p_name, 0);
	items[p_name] = current + p_quantity;
}

bool Inventory::try_consume(const String &p_name, int p_quantity) {
	int current = items.get(p_name, 0);
	if (current >= p_quantity) {
		items[p_name] = current - p_quantity;
		return true;
	}
	return false;
}

int Inventory::get_item_quantity(const String &p_name) const {
	return items.get(p_name, 0);
}

bool Inventory::has_item(const String &p_name, int p_min_quantity) const {
	int current = items.get(p_name, 0);
	return current >= p_min_quantity;
}

void Inventory::clear() {
	items.clear();
}

void Inventory::set_items(const Dictionary &p_items) {
	items = p_items;
}

Dictionary Inventory::get_items() const {
	return items;
}

String Inventory::to_json() const {
	return JSON::stringify(items);
}

void Inventory::from_json(const String &p_json) {
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(p_json) == OK) {
		items = json->get_data();
	}
}

} // namespace godot
