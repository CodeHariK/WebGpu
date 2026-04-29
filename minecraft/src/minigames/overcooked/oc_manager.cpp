#include "oc_manager.h"
#include "oc_ingredient.h"
#include "oc_station.h"
#include <algorithm>
#include <godot_cpp/variant/utility_functions.hpp>

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

} // namespace godot
