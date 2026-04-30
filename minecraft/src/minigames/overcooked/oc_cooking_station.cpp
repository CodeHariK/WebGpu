#include "oc_cooking_station.h"
#include "oc_ingredient.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void OCCookingStation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_cook_speed", "speed"), &OCCookingStation::set_cook_speed);
	ClassDB::bind_method(D_METHOD("get_cook_speed"), &OCCookingStation::get_cook_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cook_speed"), "set_cook_speed", "get_cook_speed");

	ClassDB::bind_method(D_METHOD("set_burn_speed", "speed"), &OCCookingStation::set_burn_speed);
	ClassDB::bind_method(D_METHOD("get_burn_speed"), &OCCookingStation::get_burn_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "burn_speed"), "set_burn_speed", "get_burn_speed");
}

OCCookingStation::OCCookingStation() {
	station_name = "Stove";
}

OCCookingStation::~OCCookingStation() {}

void OCCookingStation::_process(double delta) {
	OCStation::_process(delta);

	if (!held_item)
		return;

	OCIngredient *ingredient = Object::cast_to<OCIngredient>(held_item);
	if (!ingredient)
		return;

	OCIngredient::State state = ingredient->get_state();
	float progress = ingredient->get_process_progress();

	if (ingredient->can_be_cooked()) {
		// Cooking phase
		float new_progress = progress + (cook_speed * delta);
		if (new_progress >= 1.0f) {
			ingredient->set_state(OCIngredient::STATE_COOKED);
			ingredient->set_process_progress(0.0f);
			UtilityFunctions::print("Ingredient Cooked!");
		} else {
			ingredient->set_process_progress(new_progress);
		}
	} else if (state == OCIngredient::STATE_COOKED) {
		// Burning phase
		float new_progress = progress + (burn_speed * delta);
		if (new_progress >= 1.0f) {
			ingredient->set_state(OCIngredient::STATE_BURNT);
			ingredient->set_process_progress(0.0f);
			UtilityFunctions::print("Ingredient Burnt!");
		} else {
			ingredient->set_process_progress(new_progress);
		}
	}
}

void OCCookingStation::set_cook_speed(float p_speed) { cook_speed = p_speed; }
float OCCookingStation::get_cook_speed() const { return cook_speed; }

void OCCookingStation::set_burn_speed(float p_speed) { burn_speed = p_speed; }
float OCCookingStation::get_burn_speed() const { return burn_speed; }

} // namespace godot
