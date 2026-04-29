#include "overcooked_manager.h"
#include "counter_station.h"
#include "ingredient.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

namespace godot {

OvercookedManager *OvercookedManager::singleton = nullptr;

void OvercookedManager::_bind_methods() {}

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

void OvercookedManager::register_station(CounterStation *p_station) {
	stations.push_back(p_station);
}

void OvercookedManager::unregister_station(CounterStation *p_station) {
	stations.erase(std::remove(stations.begin(), stations.end(), p_station), stations.end());
}

void OvercookedManager::register_ingredient(Ingredient *p_ing) {
	ingredients.push_back(p_ing);
}

void OvercookedManager::unregister_ingredient(Ingredient *p_ing) {
	ingredients.erase(std::remove(ingredients.begin(), ingredients.end(), p_ing), ingredients.end());
}

CounterStation *OvercookedManager::get_closest_station(const Vector3 &p_from, float p_max_dist) {
	CounterStation *closest = nullptr;
	float min_dist_sq = p_max_dist * p_max_dist;

	for (auto s : stations) {
		if (!s || !s->is_inside_tree()) continue;
		float dist_sq = p_from.distance_squared_to(s->get_global_position());
		if (dist_sq < min_dist_sq) {
			min_dist_sq = dist_sq;
			closest = s;
		}
	}
	return closest;
}

Ingredient *OvercookedManager::get_closest_ingredient(const Vector3 &p_from, float p_max_dist) {
	Ingredient *closest = nullptr;
	float min_dist_sq = p_max_dist * p_max_dist;

	for (auto i : ingredients) {
		if (!i || !i->is_inside_tree() || i->get_is_picked_up()) continue;
		float dist_sq = p_from.distance_squared_to(i->get_global_position());
		if (dist_sq < min_dist_sq) {
			min_dist_sq = dist_sq;
			closest = i;
		}
	}
	return closest;
}

} // namespace godot
