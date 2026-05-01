#ifndef INVENTORY_H
#define INVENTORY_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

/**
 * @brief Generic Inventory system for managing items/resources.
 * Can be used for ingredients, ammo, crafting materials, etc.
 */
class Inventory : public Node {
	GDCLASS(Inventory, Node)

private:
	Dictionary items;

protected:
	static void _bind_methods();

public:
	Inventory();
	~Inventory();

	// Core API
	void add_item(const String &p_name, int p_quantity);
	bool try_consume(const String &p_name, int p_quantity);
	int get_item_quantity(const String &p_name) const;
	bool has_item(const String &p_name, int p_min_quantity = 1) const;
	
	void clear();

	// Bulk Operations
	void set_items(const Dictionary &p_items);
	Dictionary get_items() const;

	// Serialization helpers
	String to_json() const;
	void from_json(const String &p_json);
};

} // namespace godot

#endif // INVENTORY_H
