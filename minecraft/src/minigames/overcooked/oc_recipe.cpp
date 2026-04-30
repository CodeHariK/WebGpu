#include "oc_recipe.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void OCRecipe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_dish_name", "name"), &OCRecipe::set_dish_name);
	ClassDB::bind_method(D_METHOD("get_dish_name"), &OCRecipe::get_dish_name);
	
	ClassDB::bind_method(D_METHOD("set_required_types", "types"), &OCRecipe::set_required_types);
	ClassDB::bind_method(D_METHOD("get_required_types"), &OCRecipe::get_required_types);
	
	ClassDB::bind_method(D_METHOD("set_required_states", "states"), &OCRecipe::set_required_states);
	ClassDB::bind_method(D_METHOD("get_required_states"), &OCRecipe::get_required_states);

	ClassDB::bind_method(D_METHOD("set_base_time", "time"), &OCRecipe::set_base_time);
	ClassDB::bind_method(D_METHOD("get_base_time"), &OCRecipe::get_base_time);
	ClassDB::bind_method(D_METHOD("set_points", "points"), &OCRecipe::set_points);
	ClassDB::bind_method(D_METHOD("get_points"), &OCRecipe::get_points);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "dish_name"), "set_dish_name", "get_dish_name");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "required_types"), "set_required_types", "get_required_types");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "required_states"), "set_required_states", "get_required_states");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "base_time"), "set_base_time", "get_base_time");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "points"), "set_points", "get_points");
}

OCRecipe::OCRecipe() {}
OCRecipe::~OCRecipe() {}

std::vector<OCRecipeRequirement> OCRecipe::get_requirements() const {
	std::vector<OCRecipeRequirement> reqs;
	int count = std::min(required_types.size(), required_states.size());
	for (int i = 0; i < count; i++) {
		OCRecipeRequirement r;
		r.type = required_types[i];
		r.state = required_states[i];
		reqs.push_back(r);
	}
	return reqs;
}

} // namespace godot
