#include "oc_station.h"
#include "../../debug_draw/debug_manager.h"
#include "oc_ingredient.h"
#include "oc_manager.h"
#include "oc_plate.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/marker3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void OCStation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_station_type", "type"), &OCStation::set_station_type);
	ClassDB::bind_method(D_METHOD("get_station_type"), &OCStation::get_station_type);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "station_type", PROPERTY_HINT_ENUM, "Counter,Cutting,Cooking,Blender,Crate,Delivery,Trash"), "set_station_type", "get_station_type");

	ClassDB::bind_method(D_METHOD("place_item", "item"), &OCStation::place_item);
	ClassDB::bind_method(D_METHOD("take_item"), &OCStation::take_item);
	ClassDB::bind_method(D_METHOD("has_item"), &OCStation::has_item);

	ClassDB::bind_integer_constant("OCStation", "StationType", "TYPE_COUNTER", TYPE_COUNTER);
	ClassDB::bind_integer_constant("OCStation", "StationType", "TYPE_CUTTING", TYPE_CUTTING);
	ClassDB::bind_integer_constant("OCStation", "StationType", "TYPE_COOKING", TYPE_COOKING);
	ClassDB::bind_integer_constant("OCStation", "StationType", "TYPE_BLENDER", TYPE_BLENDER);
	ClassDB::bind_integer_constant("OCStation", "StationType", "TYPE_CRATE", TYPE_CRATE);
	ClassDB::bind_integer_constant("OCStation", "StationType", "TYPE_DELIVERY", TYPE_DELIVERY);
	ClassDB::bind_integer_constant("OCStation", "StationType", "TYPE_TRASH", TYPE_TRASH);
}

OCStation::OCStation() {}

OCStation::~OCStation() {}

void OCStation::_exit_tree() {
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (om) {
		om->unregister_station(this);
	}
}

void OCStation::_ready() {
	Interactable::_ready();
	set_is_interactable(true);

	om = OvercookedManager::get_singleton();
	if (om) {
		om->register_station(this);
	}

	// Default Visuals if none exist
	if (find_child("MeshInstance3D", true, false) == nullptr) {
		MeshInstance3D *mesh = memnew(MeshInstance3D);
		BoxMesh *bmesh = memnew(BoxMesh);
		bmesh->set_size(Vector3(1, 0.8, 1));
		mesh->set_mesh(bmesh);
		mesh->set_position(Vector3(0, 0.4, 0));
		add_child(mesh);

		// Color based on type
		StandardMaterial3D *mat = memnew(StandardMaterial3D);
		if (station_type == TYPE_CUTTING)
			mat->set_albedo(Color(0.2, 0.8, 0.2));
		else if (station_type == TYPE_COOKING)
			mat->set_albedo(Color(0.8, 0.2, 0.2));
		else if (station_type == TYPE_CRATE)
			mat->set_albedo(Color(0.2, 0.2, 0.8));
		else if (station_type == TYPE_DELIVERY)
			mat->set_albedo(Color(0.8, 0.8, 0.2));
		else if (station_type == TYPE_TRASH)
			mat->set_albedo(Color(0.3, 0.3, 0.3));
		else if (station_type == TYPE_COUNTER)
			mat->set_albedo(Color(0.5, 0.4, 0.3));
		mesh->set_material_override(mat);
	}

	if (find_child("CollisionShape3D", true, false) == nullptr) {
		CollisionShape3D *col = memnew(CollisionShape3D);
		BoxShape3D *shape = memnew(BoxShape3D);
		shape->set_size(Vector3(1, 0.8, 1));
		col->set_shape(shape);
		col->set_position(Vector3(0, 0.4, 0));
		add_child(col);
	}

	// Default Steps
	if (steps.empty()) {
		if (station_type == TYPE_CUTTING) {
			add_step(INGREDIENT_STATE_RAW, INGREDIENT_STATE_CHOPPED, 0.25f, false, PROCESS_CUT);
		} else if (station_type == TYPE_COOKING) {
			add_step(INGREDIENT_STATE_CHOPPED, INGREDIENT_STATE_COOKED, 0.1f, true, PROCESS_COOK);
		}
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

void OCStation::_process(double delta) {
	DebugManager *dm = DebugManager::get_singleton();
	if (dm) {
		String label = get_name();
		if (held_item) {
			label += " ^^(" + held_item->get_name() + ")";
		}
		dm->draw_text("station_" + get_name(), label, get_global_position() + Vector3(0, 3.0f, 0), 0.001f, Color(1, 1, 1));
	}

	// AUTOMATIC PROCESSING (Stove/Blender)
	if ((station_type == TYPE_COOKING || station_type == TYPE_BLENDER) && held_item) {
		// 1. Try to process the held item itself
		OCIngredient *main_ing = Object::cast_to<OCIngredient>(held_item);
		if (main_ing) {
			_process_ingredient(main_ing, delta);
		}
	}
}

void OCStation::_process_ingredient(OCIngredient *ingredient, float delta) {
	IngredientState current_state = ingredient->get_state();
	float progress = ingredient->get_process_progress();

	ProcessOperation op = PROCESS_NONE;
	if (station_type == TYPE_COOKING)
		op = PROCESS_COOK;
	else if (station_type == TYPE_BLENDER)
		op = PROCESS_BLEND;

	for (const auto &step : steps) {
		if (step.automatic && step.input_state == current_state && step.operation == op) {
			float new_progress = progress + (step.speed * delta);
			if (new_progress >= 1.0f) {
				ingredient->set_state(step.output_state);
				ingredient->set_process_progress(0.0f);
				UtilityFunctions::print("OCStation: Auto-step complete on ", ingredient->get_name(), " -> ", step.output_state);
			} else {
				ingredient->set_process_progress(new_progress);
			}
			break;
		}
	}
}

void OCStation::interact(Node3D *p_actor) {
	// MANUAL PROCESSING (Cutting Board)
	if (station_type == TYPE_CUTTING && held_item) {
		// 1. Try to process the held item itself
		OCIngredient *main_ing = Object::cast_to<OCIngredient>(held_item);
		if (main_ing) {
			_interact_ingredient(main_ing);
		}
	}
}

bool OCStation::_interact_ingredient(OCIngredient *ingredient) {
	IngredientState current_state = ingredient->get_state();

	ProcessOperation op = PROCESS_NONE;
	if (station_type == TYPE_CUTTING)
		op = PROCESS_CUT;

	for (const auto &step : steps) {
		if (!step.automatic && step.input_state == current_state && step.operation == op) {
			float new_progress = ingredient->get_process_progress() + step.speed;
			if (new_progress >= 1.0f) {
				ingredient->set_state(step.output_state);
				ingredient->set_process_progress(0.0f);
				UtilityFunctions::print("OCStation: Manual-step complete on ", ingredient->get_name(), " -> ", step.output_state);
			} else {
				ingredient->set_process_progress(new_progress);
			}
			return true;
		}
	}
	return false;
}

bool OCStation::can_process(OCIngredient *ingredient) const {
	if (!ingredient)
		return false;

	IngredientState current_state = ingredient->get_state();
	ProcessOperation op = PROCESS_NONE;

	if (station_type == TYPE_CUTTING)
		op = PROCESS_CUT;
	else if (station_type == TYPE_COOKING)
		op = PROCESS_COOK;
	else if (station_type == TYPE_BLENDER)
		op = PROCESS_BLEND;

	if (op == PROCESS_NONE)
		return false;

	for (const auto &step : steps) {
		if (step.input_state == current_state && step.operation == op) {
			return true;
		}
	}
	return false;
}

void OCStation::pickup(Node3D *p_actor) {
	// We don't actually pick up the station itself
}

bool OCStation::can_place_item(Interactable *item) {
	return held_item == nullptr && item != nullptr;
}

void OCStation::place_item(Interactable *item) {
	if (!item)
		return;

	// TRASH LOGIC
	if (station_type == TYPE_TRASH) {
		UtilityFunctions::print("OCStation: Item trashed!");
		item->queue_free();
		return;
	}

	// DELIVERY LOGIC
	if (station_type == TYPE_DELIVERY) {
		OCIngredient *ing = Object::cast_to<OCIngredient>(item);
		if (ing) {
			OvercookedManager *om = OvercookedManager::get_singleton();
			if (om && om->submit_ingredient(ing)) {
				UtilityFunctions::print("OCStation: Successful delivery!");
				item->queue_free();
				return;
			}
		}
		// If not an ingredient or failed delivery, treat as counter for now
	}

	held_item = item;
	if (held_item) {
		held_item->attach_to(item_slot);
	}

	// Mark as "picked up" and freeze so it doesn't fall through the station
	held_item->set_is_picked_up(true);
	held_item->set_freeze_enabled(true);
}

Interactable *OCStation::take_item() {
	// DYNAMIC CRATE LOGIC
	if (station_type == TYPE_CRATE && !held_item) {
		if (om) {
			std::vector<IngredientType> needed = om->get_needed_ingredients_list();
			for (IngredientType type : needed) {
				if (om->try_consume_inventory(type, 1)) {
					OCIngredient *ing = om->create_ingredient(type);
					if (ing) {
						if (is_inside_tree()) {
							get_tree()->get_current_scene()->add_child(ing);
						}
						UtilityFunctions::print("OCStation: Dynamically spawned needed ingredient: ", (int)type);
						return ing;
					}
				}
			}
			UtilityFunctions::print("OCStation: Crate has no ingredients in stock for current orders!");
			return nullptr;
		}
	}

	if (!held_item)
		return nullptr;

	Interactable *item = held_item;
	held_item = nullptr;
	UtilityFunctions::print("OCStation: ", get_name(), " released item, held_item is now NULL");

	// Allow it to be picked up by the player and re-enable physics
	item->set_is_picked_up(false);
	item->set_freeze_enabled(false);
	return item;
}

void OCStation::set_station_type(StationType p_type) {
	station_type = p_type;
}
StationType OCStation::get_station_type() const {
	return station_type;
}

void OCStation::add_step(IngredientState p_input, IngredientState p_output, float p_speed, bool p_auto, ProcessOperation p_op) {
	steps.push_back({ p_input, p_output, p_speed, p_auto, p_op });
}

void OCStation::clear_steps() {
	steps.clear();
}

} // namespace godot
