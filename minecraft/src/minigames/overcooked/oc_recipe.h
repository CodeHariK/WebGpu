#ifndef OC_RECIPE_H
#define OC_RECIPE_H

#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {

struct OCRecipeRequirement {
	String type;
	int state;
};

enum ProcessOperation {
	PROCESS_NONE = 0,
	PROCESS_CUT,
	PROCESS_COOK,
	PROCESS_BLEND
};

struct OCProcessStep {
	int input_state;
	int output_state;
	float speed;
	bool automatic;
	ProcessOperation operation;
};

class OCRecipe : public RefCounted {
	GDCLASS(OCRecipe, RefCounted)

private:
	String dish_name = "New Dish";

	std::vector<OCRecipeRequirement> requirements;

	float base_time = 60.0f;
	float time_remaining = 60.0f;
	int points = 100;

protected:
	static void _bind_methods();

public:
	OCRecipe();
	~OCRecipe();

	void set_dish_name(const String &p_name) { dish_name = p_name; }
	String get_dish_name() const { return dish_name; }

	void set_requirements(const std::vector<OCRecipeRequirement> &p_reqs) { requirements = p_reqs; }
	std::vector<OCRecipeRequirement> get_requirements() const { return requirements; }

	void set_base_time(float p_time) {
		base_time = p_time;
		time_remaining = p_time;
	}
	float get_base_time() const { return base_time; }

	void set_time_remaining(float p_time) { time_remaining = p_time; }
	float get_time_remaining() const { return time_remaining; }

	void set_points(int p_points) { points = p_points; }
	int get_points() const { return points; }

	Ref<OCRecipe> clone() const;

	// Serialization
	Dictionary to_dict() const;
	void from_dict(const Dictionary &p_dict);
};

} // namespace godot

#endif // OC_RECIPE_H
