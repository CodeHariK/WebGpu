#include "interactor.h"
#include <godot_cpp/classes/marker3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "../utils/raycast/mc_raycast.h"
#include "../minigames/overcooked/counter_station.h"

namespace godot {

void Interactor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("grab_or_drop"), &Interactor::grab_or_drop);
	ClassDB::bind_method(D_METHOD("interact"), &Interactor::interact);

	ClassDB::bind_method(D_METHOD("set_interaction_range", "range"), &Interactor::set_interaction_range);
	ClassDB::bind_method(D_METHOD("get_interaction_range"), &Interactor::get_interaction_range);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "interaction_range"), "set_interaction_range", "get_interaction_range");

	ClassDB::bind_method(D_METHOD("set_interaction_mask", "mask"), &Interactor::set_interaction_mask);
	ClassDB::bind_method(D_METHOD("get_interaction_mask"), &Interactor::get_interaction_mask);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "interaction_mask"), "set_interaction_mask", "get_interaction_mask");

	ClassDB::bind_method(D_METHOD("get_held_item"), &Interactor::get_held_item);
}

Interactor::Interactor() {}
Interactor::~Interactor() {}

void Interactor::_ready() {
	// Look for a hand marker in children, or create one
	hand_marker = Object::cast_to<Node3D>(find_child("HandMarker"));
	if (!hand_marker) {
		Marker3D *marker = memnew(Marker3D);
		marker->set_name("HandMarker");
		marker->set_position(Vector3(0, 1.2f, 1.0f)); // Typical player reach
		add_child(marker);
		hand_marker = marker;
	}
}

void Interactor::grab_or_drop() {
	if (held_item) {
		// Attempt to drop
		MCRaycastHit hit = raycast_3d(this, get_global_transform().origin, get_global_transform().origin - get_global_transform().basis.get_column(2) * interaction_range, interaction_mask);
		
		// 1. Try to place on a station
		CounterStation *station = hit.is_hit ? Object::cast_to<CounterStation>(hit.collider) : nullptr;
		if (station && station->can_place_item(held_item)) {
			station->place_item(held_item);
			held_item->drop(Object::cast_to<Node3D>(get_parent()));
			held_item = nullptr;
			return;
		}

		// 2. Otherwise drop on the floor
		Vector3 drop_pos = hit.is_hit ? hit.position : get_global_transform().origin - get_global_transform().basis.get_column(2) * interaction_range;
		
		Node *world = get_tree()->get_current_scene();
		held_item->reparent(world);
		held_item->set_global_position(drop_pos);
		held_item->drop(Object::cast_to<Node3D>(get_parent()));
		held_item = nullptr;
		
	} else {
		// Attempt to grab
		MCRaycastHit hit = raycast_3d(this, get_global_transform().origin, get_global_transform().origin - get_global_transform().basis.get_column(2) * interaction_range, interaction_mask);
		
		if (hit.is_hit) {
			// Check if we hit a counter station
			CounterStation *station = Object::cast_to<CounterStation>(hit.collider);
			if (station && station->has_item()) {
				held_item = station->take_item();
				held_item->pickup(Object::cast_to<Node3D>(get_parent()));
				held_item->reparent(hand_marker);
				held_item->set_position(Vector3(0, 0, 0));
				held_item->set_rotation(Vector3(0, 0, 0));
				return;
			}

			// Otherwise try to grab a normal item
			Interactable *item = Object::cast_to<Interactable>(hit.collider);
			if (item && item->get_is_interactable() && !item->get_is_picked_up()) {
				held_item = item;
				held_item->pickup(Object::cast_to<Node3D>(get_parent()));
				held_item->reparent(hand_marker);
				held_item->set_position(Vector3(0, 0, 0));
				held_item->set_rotation(Vector3(0, 0, 0));
			}
		}
	}
}

void Interactor::interact() {
	MCRaycastHit hit = raycast_3d(this, get_global_transform().origin, get_global_transform().origin - get_global_transform().basis.get_column(2) * interaction_range, interaction_mask);
	if (hit.is_hit) {
		Interactable *item = Object::cast_to<Interactable>(hit.collider);
		if (item) {
			item->interact(Object::cast_to<Node3D>(get_parent()));
		}
	}
}

void Interactor::set_interaction_range(float p_range) { interaction_range = p_range; }
float Interactor::get_interaction_range() const { return interaction_range; }

void Interactor::set_interaction_mask(uint32_t p_mask) { interaction_mask = p_mask; }
uint32_t Interactor::get_interaction_mask() const { return interaction_mask; }

Interactable *Interactor::get_held_item() const { return held_item; }

} // namespace godot
