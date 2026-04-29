#include "oc_cutting_station.h"
#include "oc_ingredient.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void OCCuttingStation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_chop_speed", "speed"), &OCCuttingStation::set_chop_speed);
	ClassDB::bind_method(D_METHOD("get_chop_speed"), &OCCuttingStation::get_chop_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "chop_speed"), "set_chop_speed", "get_chop_speed");
}

OCCuttingStation::OCCuttingStation() {
	station_name = "Cutting Board";
}
OCCuttingStation::~OCCuttingStation() {}

void OCCuttingStation::interact(Node3D *p_actor) {
	if (!held_item)
		return;

	// Try to chop the item if it's an ingredient
	OCIngredient *ingredient = Object::cast_to<OCIngredient>(held_item);
	if (ingredient && ingredient->get_state() == OCIngredient::STATE_RAW) {
		float current_progress = ingredient->get_process_progress();
		float new_progress = current_progress + chop_speed;

		ingredient->set_process_progress(new_progress);

		// If finished, transform the ingredient
		if (new_progress >= 1.0f) {
			ingredient->set_state(OCIngredient::STATE_CHOPPED);
			ingredient->set_process_progress(0.0f);

			UtilityFunctions::print("Ingredient Chopped!");
		}
	}
}

void OCCuttingStation::set_chop_speed(float p_speed) { chop_speed = p_speed; }
float OCCuttingStation::get_chop_speed() const { return chop_speed; }

} // namespace godot
