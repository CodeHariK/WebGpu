#include "turret_enemy.h"
#include "../../../ai/bt_composites.h"
#include "../../../ai/bt_leaves.h"
#include "../../../game_manager/game_manager.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TurretEnemy::_bind_methods() {
}

TurretEnemy::TurretEnemy() {
	set_enemy_kind("Turret");
	health = 2.0f;
	max_health = 2.0f;

	// Setup Behavior Tree
	btstore.instantiate();

	Ref<BTIsInRange> in_range = BTIsInRange::create(detection_range);
	Ref<BTActionShoot> shoot_act = BTActionShoot::create();
	Ref<BTWait> wait_act = BTWait::create(shoot_interval);

	bt_root = BTSequence::create({ in_range, shoot_act, wait_act });
}

TurretEnemy::~TurretEnemy() {}

void TurretEnemy::_physics_process(double delta) {
	if (is_dead)
		return;

	Node3D *player = Object::cast_to<Node3D>(GameManager::get_singleton()->get_active_target());
	if (player) {
		btstore->set_value("target", player);
	}

	if (bt_root.is_valid()) {
		bt_root->execute(this, btstore);
	}
}

void TurretEnemy::shoot() {
	UtilityFunctions::print("Turret: FIRE AT PLAYER!");
	// TODO: Spawn projectile here
}

} // namespace godot
