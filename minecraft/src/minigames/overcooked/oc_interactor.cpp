#include "oc_interactor.h"
#include "../../debug_draw/debug_manager.h"
#include "../../game_manager/game_manager.h"
#include "oc_ingredient.h"
#include "oc_manager.h"
#include "oc_plate.h"
#include "oc_station.h"
#include <godot_cpp/classes/character_body3d.hpp>
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
	if (Engine::get_singleton()->is_editor_hint())
		return;

	om = OvercookedManager::get_singleton();
	gm = GameManager::get_singleton();

	player_input = PlayerInput::get_singleton();

	// HandMarker: look for it or create it
	hand_marker = Object::cast_to<Node3D>(find_child("HandMarker"));
	if (!hand_marker) {
		Marker3D *marker = memnew(Marker3D);
		marker->set_name("HandMarker");
		marker->set_position(Vector3(0, 1.2f, 0.5f)); // Standard reach
		add_child(marker);
		hand_marker = marker;
		UtilityFunctions::print("OCInteractor: Created HandMarker.");
	}

	UtilityFunctions::print("OCInteractor: Initialized.");
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

		DebugManager *dm = DebugManager::get_singleton();

		// TARGET HIGHLIGHT LOGIC: If holding an ingredient, show where to take it regardless of distance
		OCIngredient *held_ing = Object::cast_to<OCIngredient>(held_item);
		if (held_ing && dm) {
			OCStation *target = om->get_closest_station_for_ingredient(get_global_position(), held_ing);
			if (target) {
				dm->draw_text("target_highlight", "GO HERE", target->get_global_position() + Vector3(0, 2.5f, 0), 0.001f, Color(0, 1, 0));
			}
		}

		// HELD PLATE LOGIC: If holding a plate, show prompt to pick up nearby items
		OCPlate *held_plate = Object::cast_to<OCPlate>(held_item);
		if (held_plate && dm) {
			OCIngredient *nearby_ing = om->get_closest_ingredient(get_global_position(), interaction_range);
			OCStation *station_near = om->get_closest_station(get_global_position(), interaction_range);
			if (!nearby_ing && station_near && station_near->has_item()) {
				nearby_ing = Object::cast_to<OCIngredient>(station_near->get_held_item());
				if (Object::cast_to<OCPlate>(nearby_ing))
					nearby_ing = nullptr; // Don't pick up plates into plates
			}

			if (nearby_ing) {
				dm->draw_text("plate_add_prompt", "[G] to add to plate", get_global_position() + Vector3(0, 2.5f, 0), 0.001f, Color(0, 1, 1));
			} else {
				dm->clear_text("plate_add_prompt");
			}
		} else if (dm) {
			dm->clear_text("plate_add_prompt");
		}

		// PROMPT LOGIC: Show if the current station can process what we have (only when close)
		OCStation *station = om->get_closest_station(get_global_position(), interaction_range);
		if (station) {
			String prompt = "";
			OCIngredient *station_ing = Object::cast_to<OCIngredient>(station->get_held_item());

			if (held_ing && station->can_place_item(held_ing) && station->can_process(held_ing)) {
				// We can place it to start processing
				prompt = String("[Q] to ") + get_station_operation_name(station->get_station_type());
			} else if (station_ing && station->can_process(station_ing)) {
				// Item is already on station
				if (station->get_station_type() == TYPE_CUTTING) {
					// Check if we are currently "chopping" (not moving)
					bool is_moving = false;
					CharacterBody3D *body = Object::cast_to<CharacterBody3D>(get_parent());
					if (body)
						is_moving = body->get_velocity().length() > 0.1f;

					if (is_moving)
						prompt = "STAY STILL TO CHOP";
					else
						prompt = "CHOPPING...";
				} else {
					// Cooking/Blender
					if (station->get_station_type() == TYPE_COOKING)
						prompt = "COOKING...";
					else
						prompt = "BLENDING...";
				}
			}

			if (prompt != "" && dm) {
				dm->draw_text("interact_prompt", prompt, station->get_global_position() + Vector3(0, 2.0f, 0), 0.001f, Color(1, 1, 0));
			}
		}
	}
}

void OCInteractor::grab_or_drop() {
	Vector3 my_pos = get_global_position();

	if (held_item) {
		// NEW: If holding a plate, try to pick up nearby items INTO the plate
		OCPlate *held_plate = Object::cast_to<OCPlate>(held_item);
		if (held_plate) {
			// 1. Try to grab a loose ingredient from the floor
			OCIngredient *ing = om->get_closest_ingredient(my_pos, interaction_range);
			if (ing) {
				if (held_plate->add_ingredient(ing)) {
					UtilityFunctions::print("OCInteractor: Picked up loose ingredient into held plate");
					return;
				}
			}

			// 2. Try to grab from a station
			OCStation *station = om->get_closest_station(my_pos, interaction_range);
			if (station && station->has_item()) {
				OCIngredient *station_ing = Object::cast_to<OCIngredient>(station->get_held_item());
				if (station_ing && !Object::cast_to<OCPlate>(station_ing)) {
					if (held_plate->add_ingredient(station_ing)) {
						station->take_item(); // Actually remove it from the station
						UtilityFunctions::print("OCInteractor: Picked up station ingredient into held plate");
						return;
					}
				}
			}
		}

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

		held_item->set_global_position(drop_pos);
		held_item->drop(Object::cast_to<Node3D>(get_parent()));
		held_item = nullptr;

	} else {
		// Attempt to grab
		UtilityFunctions::print("OCInteractor: Attempting to grab...");

		// 1. First try to grab a loose ingredient from the floor
		OCIngredient *ing = om->get_closest_ingredient(my_pos, interaction_range);
		if (ing) {
			UtilityFunctions::print("OCInteractor: Grabbing loose ingredient: ", ing->get_name());
			held_item = ing;
			held_item->pickup(Object::cast_to<Node3D>(get_parent()));
			held_item->attach_to(hand_marker);
			return;
		}

		// 2. Otherwise try to grab from a station (either an existing item or a crate)
		OCStation *station = om->get_closest_station(my_pos, interaction_range);
		if (station) {
			UtilityFunctions::print("OCInteractor: Grabbing from station: ", station->get_name());
			Interactable *item = station->take_item();
			if (item) {
				held_item = item;
				held_item->pickup(Object::cast_to<Node3D>(get_parent()));
				held_item->attach_to(hand_marker);
				return;
			}
		}
	}
}

void OCInteractor::interact(Node3D *p_actor) {
	OCStation *station = om->get_closest_station(get_global_position(), interaction_range);
	if (station) {
		// SMART INTERACT: If holding an item and station is free, place it first
		if (held_item && station->can_place_item(held_item)) {
			station->place_item(held_item);
			held_item->drop(Object::cast_to<Node3D>(get_parent()));
			held_item = nullptr;
		}

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
