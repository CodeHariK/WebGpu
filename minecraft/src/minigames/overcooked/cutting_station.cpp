#include "cutting_station.h"
#include "ingredient.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void CuttingStation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_chop_speed", "speed"), &CuttingStation::set_chop_speed);
	ClassDB::bind_method(D_METHOD("get_chop_speed"), &CuttingStation::get_chop_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "chop_speed"), "set_chop_speed", "get_chop_speed");
}

CuttingStation::CuttingStation() {}
CuttingStation::~CuttingStation() {}

void CuttingStation::interact(Node3D *p_actor) {
	if (!held_item)
		return;

	// Try to chop the item if it's an ingredient
	Ingredient *ingredient = Object::cast_to<Ingredient>(held_item);
	if (ingredient && ingredient->get_state() == Ingredient::STATE_RAW) {
		float current_progress = ingredient->get_process_progress();
		float new_progress = current_progress + chop_speed;
		
		ingredient->set_process_progress(new_progress);

		// If finished, transform the ingredient
		if (new_progress >= 1.0f) {
			ingredient->set_state(Ingredient::STATE_CHOPPED);
			ingredient->set_process_progress(0.0f);
			
			UtilityFunctions::print("Ingredient Chopped!");
		}
	}
}

void CuttingStation::set_chop_speed(float p_speed) { chop_speed = p_speed; }
float CuttingStation::get_chop_speed() const { return chop_speed; }

} // namespace godot
