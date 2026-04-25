#include "combat_states.h"
#include "../celeste_controller.h"
#include "../../game_manager/player_input.h"
#include "airborne_states.h"
#include "grounded_states.h"
#include "../../enemy/enemy_base.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void CelesteJumpKickState::enter() {
	target = controller->_find_melee_target();
	duration = 0.0f;

	if (target) {
		target_pos = target->get_global_position();
		// Look at target
		Vector3 dir = (target_pos - controller->get_global_position()).normalized();
		dir.y = 0; // Keep upright
		if (dir.length() > 0.1f) {
			controller->set_quaternion(Quaternion(Vector3(0, 0, -1), dir));
		}
	} else {
		// Miss lunge forward
		Transform3D t = controller->get_global_transform();
		target_pos = t.origin - t.basis.get_column(2).normalized() * 3.0f;
	}
}

void CelesteJumpKickState::physics_update(float delta) {
	duration += delta;
	Vector3 current_pos = controller->get_global_position();
	Vector3 to_target = target_pos - current_pos;
	float dist = to_target.length();

	if (dist < 1.0f || duration > MAX_DURATION) {
		// Hit! Reset resources if we had a target
		if (target) {
			controller->can_dash = true;
			controller->can_double_jump = true;
			UtilityFunctions::print("JumpKick: Hit target!");

			EnemyBase *eb = Object::cast_to<EnemyBase>(target);
			if (eb) {
				eb->take_damage(1.0f);
			}
		}

		if (controller->is_hovering) {
			controller->change_state(controller->idle_state);
		} else {
			controller->change_state(controller->fall_state);
		}
		return;
	}

	// Lunge movement
	Vector3 vel = to_target.normalized() * controller->melee_lunge_speed;
	controller->set_velocity(vel);
}

} // namespace godot
