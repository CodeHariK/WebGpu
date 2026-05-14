#ifndef OVERCOOKED_MANAGER_H
#define OVERCOOKED_MANAGER_H

#include "../../game_manager/inventory.h"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "oc_types.h"
#include <vector>

namespace godot {

class OCStation;
class OCIngredient;
class OCInteractor;
class Node3D;
class Marker3D;

class OCRecipe;
class OCIngredient;
class OCOrderUI;
class OCRecipeEditorUI;

class OvercookedManager : public Node {
	GDCLASS(OvercookedManager, Node)

private:
	static OvercookedManager *singleton;
	std::vector<OCStation *> stations;
	std::vector<OCIngredient *> ingredients;
	std::vector<Ref<OCRecipe>> active_orders;

	int score = 0;
	int last_revenue = 0;
	float order_timer = 0.0f;
	float next_order_interval = 15.0f;

	TypedArray<OCRecipe> available_recipes;
	String recipe_dir = "user://recipes/";
	String inventory_path = "user://inventory.json";

	Inventory *inventory = nullptr;

	OCOrderUI *order_ui = nullptr;
	OCRecipeEditorUI *recipe_editor = nullptr;

protected:
	static void _bind_methods();

public:
	OvercookedManager();
	~OvercookedManager();

	static OvercookedManager *get_singleton();

	void load_recipes_from_dir();
	void save_recipe_to_json(Ref<OCRecipe> p_recipe);

	void load_inventory();
	void save_inventory();
	Inventory *get_inventory_node() const { return inventory; }

	bool has_inventory(IngredientType p_type, int p_amount = 1) const;
	bool try_consume_inventory(IngredientType p_type, int p_amount = 1);

	void register_station(OCStation *p_station);
	void unregister_station(OCStation *p_station);

	void register_ingredient(OCIngredient *p_ing);
	void unregister_ingredient(OCIngredient *p_ing);

	OCStation *get_closest_station(const Vector3 &p_from, float p_max_dist);
	OCStation *get_closest_station_for_ingredient(const Vector3 &p_from, OCIngredient *p_ing);
	OCIngredient *get_closest_ingredient(const Vector3 &p_from, float p_max_dist);

	virtual void _ready() override;
	virtual void _process(double p_delta) override;

	bool submit_ingredient(OCIngredient *p_ing);
	void spawn_random_order();
	void spawn_order(Ref<OCRecipe> p_recipe);
	void cancel_order(int p_index);
	void delete_recipe(int p_index);

	int get_score() const { return score; }
	int get_last_revenue() const { return last_revenue; }

	void set_available_recipes(const TypedArray<OCRecipe> &p_recipes) { available_recipes = p_recipes; }
	TypedArray<OCRecipe> get_available_recipes() const { return available_recipes; }

	void register_ui(OCOrderUI *p_ui) { order_ui = p_ui; }
	void unregister_ui(OCOrderUI *p_ui) {
		if (order_ui == p_ui)
			order_ui = nullptr;
	}

	const std::vector<Ref<OCRecipe>> &get_active_orders() const { return active_orders; }

	void attach_interactor_to_player();
	void initialize_ui();
	std::vector<IngredientType> get_needed_ingredients_list() const;
	OCIngredient *create_ingredient(IngredientType p_type);
};

} // namespace godot

#endif // OVERCOOKED_MANAGER_H
