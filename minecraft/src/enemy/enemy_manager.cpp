#include "enemy_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

EnemyManager *EnemyManager::singleton = nullptr;

void EnemyManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_enemy", "node", "kind"), &EnemyManager::register_enemy);
	ClassDB::bind_method(D_METHOD("unregister_enemy", "node"), &EnemyManager::unregister_enemy);
}

EnemyManager::EnemyManager() {
	singleton = this;
}

EnemyManager::~EnemyManager() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

EnemyManager *EnemyManager::get_singleton() {
	return singleton;
}

void EnemyManager::register_enemy(Node3D *p_node, const String &p_kind) {
	for (const auto &e : enemies) {
		if (e.node == p_node) return;
	}
	
	EnemyData data;
	data.node = p_node;
	data.kind = p_kind;
	enemies.push_back(data);
	
	UtilityFunctions::print("EnemyManager: Registered ", p_kind, " at ", p_node->get_name());
}

void EnemyManager::unregister_enemy(Node3D *p_node) {
	for (auto it = enemies.begin(); it != enemies.end(); ++it) {
		if (it->node == p_node) {
			enemies.erase(it);
			break;
		}
	}
}

Node3D *EnemyManager::get_best_target(Vector3 p_origin, Vector3 p_input_dir, float p_max_range) {
	Node3D *best_target = nullptr;
	float best_score = -1.0f;

	for (auto &e : enemies) {
		if (!e.node || !e.node->is_inside_tree()) continue;

		Vector3 to_enemy = e.node->get_global_position() - p_origin;
		float dist = to_enemy.length();
		
		if (dist > p_max_range) continue;
		
		to_enemy.normalize();

		// Scoring system
		float score = 1.0f / (dist + 1.0f);
		
		if (p_input_dir.length() > 0.1f) {
			float dot = p_input_dir.dot(to_enemy);
			if (dot > 0.0f) {
				score += dot * 5.0f; // High priority for input alignment
			} else {
				score *= 0.1f; // Penalize targets behind input direction
			}
		}

		if (score > best_score) {
			best_score = score;
			best_target = e.node;
		}
	}

	return best_target;
}

} // namespace godot
