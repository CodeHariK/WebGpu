#include "oc_manager.h"
#include "oc_ingredient.h"
#include "oc_station.h"
#include "oc_recipe.h"
#include "oc_order_ui.h"
#include "oc_plate.h"
#include <algorithm>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

OvercookedManager *OvercookedManager::singleton = nullptr;

void OvercookedManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("submit_ingredient", "ingredient"), &OvercookedManager::submit_ingredient);
	ClassDB::bind_method(D_METHOD("spawn_random_order"), &OvercookedManager::spawn_random_order);
	ClassDB::bind_method(D_METHOD("get_score"), &OvercookedManager::get_score);
	
	ClassDB::bind_method(D_METHOD("set_available_recipes", "recipes"), &OvercookedManager::set_available_recipes);
	ClassDB::bind_method(D_METHOD("get_available_recipes"), &OvercookedManager::get_available_recipes);
	
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "available_recipes", PROPERTY_HINT_RESOURCE_TYPE, "OCRecipe"), "set_available_recipes", "get_available_recipes");
}

OvercookedManager::OvercookedManager() {
	if (singleton == nullptr) {
		singleton = this;
	}
}

OvercookedManager::~OvercookedManager() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

OvercookedManager *OvercookedManager::get_singleton() {
	return singleton;
}

void OvercookedManager::register_station(OCStation *p_station) {
	if (std::find(stations.begin(), stations.end(), p_station) == stations.end()) {
		stations.push_back(p_station);
	}
}

void OvercookedManager::unregister_station(OCStation *p_station) {
	auto it = std::find(stations.begin(), stations.end(), p_station);
	if (it != stations.end()) {
		stations.erase(it);
	}
}

void OvercookedManager::register_ingredient(OCIngredient *p_ing) {
	if (std::find(ingredients.begin(), ingredients.end(), p_ing) == ingredients.end()) {
		ingredients.push_back(p_ing);
	}
}

void OvercookedManager::unregister_ingredient(OCIngredient *p_ing) {
	auto it = std::find(ingredients.begin(), ingredients.end(), p_ing);
	if (it != ingredients.end()) {
		ingredients.erase(it);
	}
}

OCStation *OvercookedManager::get_closest_station(const Vector3 &p_from, float p_max_dist) {
	OCStation *closest = nullptr;
	float min_dist_sq = p_max_dist * p_max_dist;

	for (auto s : stations) {
		if (!s || !s->is_inside_tree())
			continue;

		float dist_sq = p_from.distance_squared_to(s->get_global_position());
		if (dist_sq < min_dist_sq) {
			min_dist_sq = dist_sq;
			closest = s;
		}
	}
	return closest;
}

OCIngredient *OvercookedManager::get_closest_ingredient(const Vector3 &p_from, float p_max_dist) {
	OCIngredient *closest = nullptr;
	float min_dist_sq = p_max_dist * p_max_dist;

	for (auto i : ingredients) {
		if (!i || !i->is_inside_tree() || i->get_is_picked_up())
			continue;

		float dist_sq = p_from.distance_squared_to(i->get_global_position());
		if (dist_sq < min_dist_sq) {
			min_dist_sq = dist_sq;
			closest = i;
		}
	}
	return closest;
}

void OvercookedManager::_process(double delta) {
	// 1. Handle Order Timers
	for (int i = (int)active_orders.size() - 1; i >= 0; i--) {
		active_orders[i].time_remaining -= delta;
		if (active_orders[i].time_remaining <= 0) {
			UtilityFunctions::print("OCManager: Order Failed: ", active_orders[i].dish_name);
			active_orders.erase(active_orders.begin() + i);
			// We rebuild UI or notify it
		}
	}

	// 2. Spawn New Orders
	order_spawn_timer += delta;
	if (order_spawn_timer >= next_order_interval) {
		order_spawn_timer = 0.0f;
		if (active_orders.size() < 5) { // Limit concurrent orders
			spawn_random_order();
		}
	}
}

void OvercookedManager::spawn_random_order() {
	if (available_recipes.size() == 0) return;

	int idx = rand() % available_recipes.size();
	Ref<OCRecipe> recipe = available_recipes[idx];
	if (!recipe.is_valid()) return;

	OCOrder new_order;
	new_order.dish_name = recipe->get_dish_name();
	new_order.requirements = recipe->get_requirements();
	new_order.total_time = recipe->get_base_time();
	new_order.time_remaining = new_order.total_time;
	new_order.points = recipe->get_points();

	active_orders.push_back(new_order);
	UtilityFunctions::print("OCManager: New Order: ", new_order.dish_name);
}

bool OvercookedManager::submit_ingredient(OCIngredient *p_ing) {
	if (!p_ing) return false;

	// Collect ingredients to check
	std::vector<OCIngredient*> submitted_items;
	OCPlate *plate = Object::cast_to<OCPlate>(p_ing);
	
	if (plate) {
		submitted_items = plate->get_contents();
	} else {
		submitted_items.push_back(p_ing);
	}

	for (int i = 0; i < (int)active_orders.size(); i++) {
		const auto& reqs = active_orders[i].requirements;
		
		if (reqs.size() != submitted_items.size()) continue;

		// Matching Logic
		std::vector<OCRecipeRequirement> remaining = reqs;
		bool match = true;
		
		for (OCIngredient* item : submitted_items) {
			bool found = false;
			for (auto it = remaining.begin(); it != remaining.end(); ++it) {
				if (it->type == item->get_ingredient_type() && it->state == (int)item->get_state()) {
					remaining.erase(it);
					found = true;
					break;
				}
			}
			if (!found) {
				match = false;
				break;
			}
		}

		if (match && remaining.empty()) {
			// Success!
			score += active_orders[i].points;
			UtilityFunctions::print("OCManager: Order Complete! +", active_orders[i].points, " pts. Total: ", score);
			
			active_orders.erase(active_orders.begin() + i);
			
			// If it was a plate, we don't necessarily delete the plate, just clear it?
			// Or delete everything submitted.
			if (plate) {
				plate->clear_contents();
			} else {
				p_ing->queue_free();
			}

			return true;
		}
	}

	return false;
}

} // namespace godot
