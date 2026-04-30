#include "oc_interactor.h"
#include "../../game_manager/game_manager.h"
#include "oc_ingredient.h"
#include "oc_manager.h"
#include "oc_station.h"
#include "oc_plate.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/marker3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void OCInteractor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("grab_or_drop"), &OCInteractor::grab_or_drop);
	ClassDB::bind_method(D_METHOD("interact"), &OCInteractor::interact);

	ClassDB::bind_method(D_METHOD("set_interaction_range", "range"), &OCInteractor::set_interaction_range);
	ClassDB::bind_method(D_METHOD("get_interaction_range"), &OCInteractor::get_interaction_range);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "interaction_range"), "set_interaction_range", "get_interaction_range");

	ClassDB::bind_method(D_METHOD("set_interaction_mask", "mask"), &OCInteractor::set_interaction_mask);
	ClassDB::bind_method(D_METHOD("get_interaction_mask"), &OCInteractor::get_interaction_mask);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "interaction_mask"), "set_interaction_mask", "get_interaction_mask");

	ClassDB::bind_method(D_METHOD("get_held_item"), &OCInteractor::get_held_item);
}

OCInteractor::OCInteractor() {}
OCInteractor::~OCInteractor() {}

void OCInteractor::_ready() {
	// Look for a hand marker in children, or create one
	hand_marker = Object::cast_to<Node3D>(find_child("HandMarker"));
	if (!hand_marker) {
		Marker3D *marker = memnew(Marker3D);
		marker->set_name("HandMarker");
		marker->set_position(Vector3(0, 1.2f, 0.5f)); // Typical player reach
		add_child(marker);
		hand_marker = marker;
	}

	gm = GameManager::get_singleton();
	if (gm) {
		player_input = gm->get_player_input();
	}

	om = OvercookedManager::get_singleton();
}

void OCInteractor::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	// Only process input if parent is the active target
	if (!gm || gm->get_active_target() != get_parent()) {
		return;
	}

	if (player_input) {
		const ActionState &state = player_input->get_state();
		if (state.character.grab_just_pressed) {
			UtilityFunctions::print("OCInteractor: Grab key pressed");
			grab_or_drop();
		}
		if (state.character.interact_just_pressed) {
			UtilityFunctions::print("OCInteractor: Interact key pressed");
			interact();
		}
	}
}

void OCInteractor::grab_or_drop() {
	Vector3 my_pos = get_global_position();

	if (held_item) {
		// Attempt to drop
		UtilityFunctions::print("OCInteractor: Attempting to drop item: ", held_item->get_name());

		// 1. Try to place on a station
		OCStation *station = om->get_closest_station(my_pos, interaction_range);
		if (station) {
			UtilityFunctions::print("OCInteractor: Closest station found: ", station->get_name());
			
			// NEW: If station has a plate, add to it!
			if (station->has_item()) {
				OCPlate *plate = Object::cast_to<OCPlate>(station->get_held_item());
				OCIngredient *ing_to_add = Object::cast_to<OCIngredient>(held_item);
				if (plate && ing_to_add && !Object::cast_to<OCPlate>(held_item)) {
					if (plate->add_ingredient(ing_to_add)) {
						held_item->drop(Object::cast_to<Node3D>(get_parent()));
						held_item = nullptr;
						return;
					}
				}
			}

			if (station->can_place_item(held_item)) {
				UtilityFunctions::print("OCInteractor: Placing item on station");
				station->place_item(held_item);
				held_item->drop(Object::cast_to<Node3D>(get_parent()));
				held_item = nullptr;
				return;
			}
		}

		// 2. Otherwise drop on the floor
		UtilityFunctions::print("OCInteractor: Dropping on floor");
		Node *world = get_tree()->get_current_scene();
		held_item->reparent(world);
		
		// Drop in front of the player, but at a pickable height
		Vector3 drop_pos = my_pos + (get_global_transform().basis.get_column(2) * -1.5f);
		drop_pos.y = 1.0f; // Ensure it's not buried in the floor
		
		held_item->set_global_position(drop_pos);
		held_item->drop(Object::cast_to<Node3D>(get_parent()));
		held_item = nullptr;

	} else {
		// Attempt to grab
		UtilityFunctions::print("OCInteractor: Attempting to grab...");

		// 1. Check for closest station with an item
		OCStation *station = om->get_closest_station(my_pos, interaction_range);
		if (station && station->has_item()) {
			UtilityFunctions::print("OCInteractor: Grabbing from station: ", station->get_name());
			held_item = station->take_item();
			held_item->pickup(Object::cast_to<Node3D>(get_parent()));
			held_item->reparent(hand_marker);
			held_item->set_position(Vector3(0, 0, 0));
			held_item->set_rotation(Vector3(0, 0, 0));
			return;
		}

		// 2. Otherwise try to grab a loose ingredient
		OCIngredient *ing = om->get_closest_ingredient(my_pos, interaction_range);
		if (ing) {
			UtilityFunctions::print("OCInteractor: Grabbing loose ingredient: ", ing->get_name());
			held_item = ing;
			held_item->pickup(Object::cast_to<Node3D>(get_parent()));
			held_item->reparent(hand_marker);
			held_item->set_position(Vector3(0, 0, 0));
			held_item->set_rotation(Vector3(0, 0, 0));
		}
	}
}

void OCInteractor::interact(Node3D *p_actor) {
	OCStation *station = om->get_closest_station(get_global_position(), interaction_range);
	if (station) {
		UtilityFunctions::print("OCInteractor: Interacting with station: ", station->get_name());
		Node3D *actor = p_actor ? p_actor : Object::cast_to<Node3D>(get_parent());
		station->interact(actor);
	}
}

void OCInteractor::set_interaction_range(float p_range) { interaction_range = p_range; }
float OCInteractor::get_interaction_range() const { return interaction_range; }

void OCInteractor::set_interaction_mask(uint32_t p_mask) { interaction_mask = p_mask; }
uint32_t OCInteractor::get_interaction_mask() const { return interaction_mask; }

Interactable *OCInteractor::get_held_item() const { return held_item; }

} // namespace godot
