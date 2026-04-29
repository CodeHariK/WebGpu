#include "interactor.h"
#include <godot_cpp/classes/marker3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "../utils/raycast/mc_raycast.h"
#include "../minigames/overcooked/counter_station.h"
#include "../minigames/overcooked/ingredient.h"
#include "../minigames/overcooked/overcooked_manager.h"
#include "../game_manager/game_manager.h"

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
		marker->set_position(Vector3(0, 1.2f, 0.5f)); // Typical player reach
		add_child(marker);
		hand_marker = marker;
	}
}

void Interactor::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) return;

	// Only process input if parent is the active target
	GameManager *gm = GameManager::get_singleton();
	if (!gm || gm->get_active_target() != get_parent()) {
		return;
	}

	Input *input = Input::get_singleton();
	if (input->is_action_just_pressed("grab")) {
		UtilityFunctions::print("Interactor: Grab key pressed");
		grab_or_drop();
	}
	if (input->is_action_just_pressed("interact")) {
		UtilityFunctions::print("Interactor: Interact key pressed");
		interact();
	}
}

void Interactor::grab_or_drop() {
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (!om) return;

	Vector3 my_pos = get_global_position();

	if (held_item) {
		// Attempt to drop
		UtilityFunctions::print("Interactor: Attempting to drop item: ", held_item->get_name());
		
		// 1. Try to place on a station
		CounterStation *station = om->get_closest_station(my_pos, interaction_range);
		if (station) {
			UtilityFunctions::print("Interactor: Closest station found: ", station->get_name());
			if (station->can_place_item(held_item)) {
				UtilityFunctions::print("Interactor: Placing item on station");
				station->place_item(held_item);
				held_item->drop(Object::cast_to<Node3D>(get_parent()));
				held_item = nullptr;
				return;
			}
		}

		// 2. Otherwise drop on the floor
		UtilityFunctions::print("Interactor: Dropping on floor");
		Node *world = get_tree()->get_current_scene();
		held_item->reparent(world);
		held_item->set_global_position(my_pos + (get_global_transform().basis.get_column(2) * -1.0f));
		held_item->drop(Object::cast_to<Node3D>(get_parent()));
		held_item = nullptr;
		
	} else {
		// Attempt to grab
		UtilityFunctions::print("Interactor: Attempting to grab...");
		
		// 1. Check for closest station with an item
		CounterStation *station = om->get_closest_station(my_pos, interaction_range);
		if (station && station->has_item()) {
			UtilityFunctions::print("Interactor: Grabbing from station: ", station->get_name());
			held_item = station->take_item();
			held_item->pickup(Object::cast_to<Node3D>(get_parent()));
			held_item->reparent(hand_marker);
			held_item->set_position(Vector3(0, 0, 0));
			held_item->set_rotation(Vector3(0, 0, 0));
			return;
		}

		// 2. Otherwise try to grab a loose ingredient
		Ingredient *ing = om->get_closest_ingredient(my_pos, interaction_range);
		if (ing) {
			UtilityFunctions::print("Interactor: Grabbing loose ingredient: ", ing->get_name());
			held_item = ing;
			held_item->pickup(Object::cast_to<Node3D>(get_parent()));
			held_item->reparent(hand_marker);
			held_item->set_position(Vector3(0, 0, 0));
			held_item->set_rotation(Vector3(0, 0, 0));
		}
	}
}

void Interactor::interact(Node3D *p_actor) {
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (!om) return;

	CounterStation *station = om->get_closest_station(get_global_position(), interaction_range);
	if (station) {
		UtilityFunctions::print("Interactor: Interacting with station: ", station->get_name());
		Node3D *actor = p_actor ? p_actor : Object::cast_to<Node3D>(get_parent());
		station->interact(actor);
	}
}

void Interactor::set_interaction_range(float p_range) { interaction_range = p_range; }
float Interactor::get_interaction_range() const { return interaction_range; }

void Interactor::set_interaction_mask(uint32_t p_mask) { interaction_mask = p_mask; }
uint32_t Interactor::get_interaction_mask() const { return interaction_mask; }

Interactable *Interactor::get_held_item() const { return held_item; }

} // namespace godot
