#ifndef OC_RECIPE_H
#define OC_RECIPE_H

#include <godot_cpp/classes/resource.hpp>
#include "oc_manager.h"

namespace godot {

class OCRecipe : public Resource {
	GDCLASS(OCRecipe, Resource)

private:
	String dish_name = "New Dish";
	
	// We'll use two parallel arrays or a custom resource to expose this to the editor
	// For simplicity in the C++ API, I'll use TypedArrays
	PackedStringArray required_types;
	PackedInt32Array required_states;
	
	float base_time = 60.0f;
	int points = 100;

protected:
	static void _bind_methods();

public:
	OCRecipe();
	~OCRecipe();

	void set_dish_name(const String &p_name) { dish_name = p_name; }
	String get_dish_name() const { return dish_name; }

	void set_required_types(const PackedStringArray &p_types) { required_types = p_types; }
	PackedStringArray get_required_types() const { return required_types; }

	void set_required_states(const PackedInt32Array &p_states) { required_states = p_states; }
	PackedInt32Array get_required_states() const { return required_states; }

	void set_base_time(float p_time) { base_time = p_time; }
	float get_base_time() const { return base_time; }

	void set_points(int p_points) { points = p_points; }
	int get_points() const { return points; }

	// Helper for C++ logic
	std::vector<OCRecipeRequirement> get_requirements() const;
};

} // namespace godot

#endif // OC_RECIPE_H
