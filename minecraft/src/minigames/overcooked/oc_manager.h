#ifndef OVERCOOKED_MANAGER_H
#define OVERCOOKED_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <vector>

namespace godot {

class OCStation;
class OCIngredient;

class OCRecipe;
class OCIngredient;
class OCOrderUI;

struct OCRecipeRequirement {
	String type;
	int state;
};

struct OCOrder {
	String dish_name;
	std::vector<OCRecipeRequirement> requirements;
	float time_remaining;
	float total_time;
	int points;
};

class OvercookedManager : public Node {
	GDCLASS(OvercookedManager, Node)

private:
	static OvercookedManager *singleton;
	std::vector<OCStation *> stations;
	std::vector<OCIngredient *> ingredients;
	std::vector<OCOrder> active_orders;
	
	int score = 0;
	float order_spawn_timer = 0.0f;
	float next_order_interval = 15.0f;
	
	TypedArray<OCRecipe> available_recipes;
	OCOrderUI *order_ui = nullptr;

protected:
	static void _bind_methods();

public:
	OvercookedManager();
	~OvercookedManager();

	static OvercookedManager *get_singleton();

	void register_station(OCStation *p_station);
	void unregister_station(OCStation *p_station);

	void register_ingredient(OCIngredient *p_ing);
	void unregister_ingredient(OCIngredient *p_ing);

	OCStation *get_closest_station(const Vector3 &p_from, float p_max_dist);
	OCIngredient *get_closest_ingredient(const Vector3 &p_from, float p_max_dist);

	void _process(double delta) override;
	
	bool submit_ingredient(OCIngredient *p_ing);
	void spawn_random_order();
	
	int get_score() const { return score; }
	
	void set_available_recipes(const TypedArray<OCRecipe> &p_recipes) { available_recipes = p_recipes; }
	TypedArray<OCRecipe> get_available_recipes() const { return available_recipes; }

	void register_ui(OCOrderUI *p_ui) { order_ui = p_ui; }
	void unregister_ui(OCOrderUI *p_ui) { if (order_ui == p_ui) order_ui = nullptr; }
	
	const std::vector<OCOrder>& get_active_orders() const { return active_orders; }
};

} // namespace godot

#endif // OVERCOOKED_MANAGER_H
