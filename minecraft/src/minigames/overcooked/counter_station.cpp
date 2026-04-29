#include "counter_station.h"
#include "../../debug_draw/debug_manager.h"
#include "overcooked_manager.h"
#include <godot_cpp/classes/marker3d.hpp>

namespace godot {

void CounterStation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_station_name", "name"), &CounterStation::set_station_name);
	ClassDB::bind_method(D_METHOD("get_station_name"), &CounterStation::get_station_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "station_name"), "set_station_name", "get_station_name");
	ClassDB::bind_method(D_METHOD("place_item", "item"), &CounterStation::place_item);
	ClassDB::bind_method(D_METHOD("take_item"), &CounterStation::take_item);
	ClassDB::bind_method(D_METHOD("has_item"), &CounterStation::has_item);
}

CounterStation::CounterStation() {
	set_is_interactable(true);
}

CounterStation::~CounterStation() {}

void CounterStation::_exit_tree() {
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (om) {
		om->unregister_station(this);
	}
}

void CounterStation::_ready() {
	Interactable::_ready();

	OvercookedManager *om = OvercookedManager::get_singleton();
	if (om) {
		om->register_station(this);
	}

	// Look for a specific slot to place items
	item_slot = Object::cast_to<Node3D>(find_child("ItemSlot"));
	if (!item_slot) {
		Marker3D *marker = memnew(Marker3D);
		marker->set_name("ItemSlot");
		marker->set_position(Vector3(0, 1.0f, 0)); // Default counter height
		add_child(marker);
		item_slot = marker;
	}
	if (item_slot == nullptr) {
		item_slot = this;
	}
}

void CounterStation::_process(double delta) {
	DebugManager *dm = DebugManager::get_singleton();
	if (dm) {
		String label = station_name;
		if (held_item) {
			label += "\n(" + held_item->get_name() + ")";
		}
		dm->draw_text("station_" + get_name(), label, get_global_position() + Vector3(0, 3.0f, 0), 0.001f, Color(1, 1, 1));
	}
}

void CounterStation::interact(Node3D *p_actor) {
	// Base counters don't do much on interact (use)
	// Subclasses like CuttingStation will override this
}

void CounterStation::pickup(Node3D *p_actor) {
	// We don't actually pick up the counter itself
}

bool CounterStation::can_place_item(Interactable *item) {
	return held_item == nullptr && item != nullptr;
}

void CounterStation::place_item(Interactable *item) {
	if (!item)
		return;

	held_item = item;
	held_item->reparent(item_slot);
	held_item->set_position(Vector3(0, 0, 0));
	held_item->set_rotation(Vector3(0, 0, 0));

	// Mark as "picked up" so it doesn't fall or get grabbed by raycasts individually
	held_item->set_is_picked_up(true);
}

Interactable *CounterStation::take_item() {
	if (!held_item)
		return nullptr;

	Interactable *item = held_item;
	held_item = nullptr;

	// Allow it to be picked up by the player
	item->set_is_picked_up(false);
	return item;
}

void CounterStation::set_station_name(const String &p_name) { station_name = p_name; }
String CounterStation::get_station_name() const { return station_name; }

} // namespace godot
