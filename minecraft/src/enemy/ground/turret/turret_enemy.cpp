#include "turret_enemy.h"
#include "../../../game_manager/game_manager.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "../../../ai/bt_composites.h"
#include "../../../ai/bt_leaves.h"

namespace godot {

void TurretEnemy::_bind_methods() {
}

TurretEnemy::TurretEnemy() {
	set_enemy_kind("Turret");
	health = 2.0f;
	max_health = 2.0f;

	// Setup Behavior Tree
	blackboard.instantiate();
	
	Ref<BTSequence> seq;
	seq.instantiate();

	Ref<BTIsInRange> in_range;
	in_range.instantiate();
	in_range->set_range(detection_range);

	Ref<BTActionShoot> shoot_act;
	shoot_act.instantiate();

	Ref<BTWait> wait_act;
	wait_act.instantiate();
	wait_act->set_duration(shoot_interval);

	seq->add_child(in_range);
	seq->add_child(shoot_act);
	seq->add_child(wait_act);

	bt_root = seq;
}

TurretEnemy::~TurretEnemy() {}

void TurretEnemy::_physics_process(double delta) {
	if (is_dead)
		return;

	Node3D *player = Object::cast_to<Node3D>(GameManager::get_singleton()->get_active_target());
	if (player) {
		blackboard->set_value("target", player);
	}

	if (bt_root.is_valid()) {
		bt_root->execute(this, blackboard);
	}
}

void TurretEnemy::shoot() {
	UtilityFunctions::print("Turret: FIRE AT PLAYER!");
	// TODO: Spawn projectile here
}

} // namespace godot
