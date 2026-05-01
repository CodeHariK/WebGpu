#include "oc_recipe.h"
#include "oc_ingredient.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

void OCRecipe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_dish_name", "name"), &OCRecipe::set_dish_name);
	ClassDB::bind_method(D_METHOD("get_dish_name"), &OCRecipe::get_dish_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "dish_name"), "set_dish_name", "get_dish_name");

	ClassDB::bind_method(D_METHOD("set_base_time", "time"), &OCRecipe::set_base_time);
	ClassDB::bind_method(D_METHOD("get_base_time"), &OCRecipe::get_base_time);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "base_time"), "set_base_time", "get_base_time");

	ClassDB::bind_method(D_METHOD("set_points", "points"), &OCRecipe::set_points);
	ClassDB::bind_method(D_METHOD("get_points"), &OCRecipe::get_points);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "points"), "set_points", "get_points");
}

OCRecipe::OCRecipe() {}
OCRecipe::~OCRecipe() {}

Ref<OCRecipe> OCRecipe::clone() const {
	Ref<OCRecipe> copy;
	copy.instantiate();
	copy->set_dish_name(dish_name);
	copy->set_requirements(requirements);
	copy->set_base_time(base_time);
	copy->set_points(points);
	return copy;
}

Dictionary OCRecipe::to_dict() const {
	Dictionary dict;
	dict["name"] = dish_name;
	
	PackedStringArray types;
	PackedByteArray states;
	for (const auto &req : requirements) {
		types.push_back(req.type);
		states.push_back((uint8_t)req.state);
	}

	dict["types"] = types;
	dict["states"] = states;
	dict["time"] = base_time;
	dict["points"] = points;
	return dict;
}

void OCRecipe::from_dict(const Dictionary &p_dict) {
	if (p_dict.has("name"))
		dish_name = p_dict["name"];
	
	requirements.clear();
	if (p_dict.has("types") && p_dict.has("states")) {
		PackedStringArray types = p_dict["types"];
		PackedByteArray states = p_dict["states"];
		for (int i = 0; i < types.size(); i++) {
			OCRecipeRequirement req;
			req.type = types[i];
			req.state = states[i];
			requirements.push_back(req);
		}
	}

	if (p_dict.has("time"))
		base_time = (float)p_dict["time"];
	if (p_dict.has("points"))
		points = (int)p_dict["points"];
}

} // namespace godot
