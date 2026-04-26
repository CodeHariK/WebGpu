#include "patrol_melee_enemy.h"
#include "../../../game_manager/game_manager.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include "../../../ai/bt_composites.h"
#include "../../../ai/bt_leaves.h"

namespace godot {

void PatrolMeleeEnemy::_bind_methods() {
}

PatrolMeleeEnemy::PatrolMeleeEnemy() {
	set_enemy_kind("PatrolMelee");
	health = 5.0f;
	max_health = 5.0f;

	// Setup Behavior Tree
	blackboard.instantiate();
	
	// Attack Sequence
	Ref<BTIsInRange> check_melee = BTIsInRange::create(melee_range);
	Ref<BTActionMelee> do_melee = BTActionMelee::create();
	Ref<BTWait> cooldown = BTWait::create(1.0f); // 1 second cooldown after attack

	Ref<BTSequence> attack_seq = BTSequence::create({check_melee, do_melee, cooldown});

	// Approach Action
	Ref<BTActionApproach> do_approach = BTActionApproach::create(approach_speed, melee_range);

	// Attack or Approach Selector
	Ref<BTSelector> attack_or_approach_sel = BTSelector::create({attack_seq, do_approach});

	// Engage Sequence
	Ref<BTIsInRange> check_detect = BTIsInRange::create(detection_range);
	Ref<BTReactiveSequence> engage_seq = BTReactiveSequence::create({check_detect, attack_or_approach_sel});

	// Patrol Sequence
	Ref<BTPatrol> do_patrol = BTPatrol::create(patrol_speed);
	Ref<BTWait> patrol_wait = BTWait::create(2.0f); // Wait 2 seconds between patrol points
	Ref<BTSequence> patrol_seq = BTSequence::create({do_patrol, patrol_wait});

	// Assemble Root Selector
	bt_root = BTReactiveSelector::create({engage_seq, patrol_seq});
}

PatrolMeleeEnemy::~PatrolMeleeEnemy() {}

void PatrolMeleeEnemy::_physics_process(double delta) {
	if (is_dead)
		return;

	// Apply Gravity
	Vector3 vel = get_velocity();
	vel.y -= 9.8f * delta;
	set_velocity(vel);

	Node3D *player = Object::cast_to<Node3D>(GameManager::get_singleton()->get_active_target());
	if (player) {
		blackboard->set_value("target", player);
	}

	if (bt_root.is_valid()) {
		bt_root->execute(this, blackboard);
	}

	move_and_slide();
}

void PatrolMeleeEnemy::melee_attack() {
	UtilityFunctions::print("PatrolMelee: MELEE ATTACK!");
	// TODO: Trigger attack animation, deal damage, etc.
}

} // namespace godot
