#include "turret_enemy.h"
#include "../../../game_manager/game_manager.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TurretEnemy::_bind_methods() {
}

TurretEnemy::TurretEnemy() {
	set_enemy_kind("Turret");
	health = 2.0f;
	max_health = 2.0f;
}

TurretEnemy::~TurretEnemy() {}

void TurretEnemy::_physics_process(double delta) {
	if (is_dead)
		return;

	Node3D *player = Object::cast_to<Node3D>(GameManager::get_singleton()->get_active_target());
	if (!player)
		return;

	Vector3 to_player = player->get_global_position() - get_global_position();
	float dist = to_player.length();

	if (dist < detection_range) {
		shoot_timer += (float)delta;
		if (shoot_timer >= shoot_interval) {
			shoot_timer = 0.0f;
			shoot();
		}
	}
}

void TurretEnemy::shoot() {
	UtilityFunctions::print("Turret: FIRE AT PLAYER!");
	// TODO: Spawn projectile here
}

} // namespace godot
