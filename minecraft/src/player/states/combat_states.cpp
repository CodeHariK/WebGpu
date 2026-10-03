#include "combat_states.h"
#include "../../enemy/enemy_base.h"
#include "../celeste_controller.h"
#include "airborne_states.h"
#include "grounded_states.h"

#include <godot_cpp/classes/kinematic_collision3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/object.hpp>

#include <cmath>

namespace godot {

// --- Spin attack ---
static const float SPIN_DURATION = 0.32f; // whole spin
static const float SPIN_HIT_TIME = 0.06f; // damage lands this far in (reads as the spin connecting)
static const float AIR_POP = 4.0f; // upward speed given by an air spin (m/s)
static const float AIR_GRAVITY_SCALE = 0.35f; // floaty hang during an air spin
static const float AIR_DRAG = 3.0f; // horizontal slow-down in the air (1/s)
static const float GROUND_BRAKE = 2.0f; // x friction while planted

// --- Dive kick ---
static const float DIVE_WINDUP = 0.12f; // hang + lean back before the dive (the tell)
static const float DIVE_HANG_VY = 1.5f; // small lift at the start of the wind-up (m/s)
static const float DIVE_SPEED = 24.0f; // m/s
static const float DIVE_MAX_TIME = 0.8f; // give up after this long
static const float DIVE_HIT_DIST = 1.6f; // centre-to-centre contact (capsule + enemy half sizes)
static const float DIVE_DAMAGE = 1.0f;
static const float BOUNCE_UP = 0.55f; // bounce height as a fraction of the jump launch speed
static const float BOUNCE_BACK = 3.0f; // pushed back off the enemy (m/s)
static const float WINDUP_LEAN = -0.35f; // tuck forward (rad): anticipation before the kick
static const float KICK_LEAN_MIN = 0.15f; // straight-down dive: nearly upright stomp (rad)
static const float KICK_LEAN_MAX = 1.2f; // flat dive: leaning far back, a flying dropkick (rad)

// =============================================================================
// Spin attack
// =============================================================================

void CelesteAttackState::enter() {
	time = 0.0f;
	hit_done = false;
	airborne = !controller->is_hovering;

	if (airborne && controller->can_air_spin) {
		Vector3 v = controller->get_velocity();
		v.y = MAX(v.y, AIR_POP);
		controller->set_velocity(v);
		controller->can_air_spin = false;
	}
	controller->attack_fx.play_spin(controller->melee_range);
}

void CelesteAttackState::exit() {
	controller->attack_fx.stop();
}

void CelesteAttackState::physics_update(float delta) {
	time += delta;

	Vector3 v = controller->get_velocity();
	if (airborne) {
		v.x *= MAX(0.0f, 1.0f - AIR_DRAG * delta);
		v.z *= MAX(0.0f, 1.0f - AIR_DRAG * delta);
		float g = (v.y > 0.0f) ? controller->_jump_gravity : controller->_fall_gravity;
		v.y -= g * AIR_GRAVITY_SCALE * delta;
	} else {
		float brake = controller->friction * GROUND_BRAKE * delta;
		v.x = Math::move_toward(v.x, 0.0f, brake);
		v.z = Math::move_toward(v.z, 0.0f, brake);
		// vertical: left to the hover spring
	}
	controller->set_velocity(v);

	if (!hit_done && time >= SPIN_HIT_TIME) {
		hit_done = true;
		controller->_spin_hit();
	}
	controller->attack_fx.update_spin(time / SPIN_DURATION);

	if (time >= SPIN_DURATION) {
		controller->change_state(controller->is_hovering ? (CelesteState *)controller->idle_state
														 : (CelesteState *)controller->fall_state);
	}
}

// =============================================================================
// Dive kick
// =============================================================================

Node3D *CelesteDiveKickState::_target() const {
	Node3D *n = Object::cast_to<Node3D>(ObjectDB::get_instance(target_id));
	if (!n || !n->is_inside_tree() || n->is_queued_for_deletion()) {
		return nullptr;
	}
	EnemyBase *eb = Object::cast_to<EnemyBase>(n);
	return (eb && eb->get_is_dead()) ? nullptr : n;
}

void CelesteDiveKickState::enter() {
	target_id = controller->_dive_target_id;
	time = 0.0f;
	controller->is_jumping = true; // hover spring off: the dive owns the vertical motion
	controller->set_velocity(Vector3(0.0f, DIVE_HANG_VY, 0.0f));
	controller->attack_fx.play_dive();
}

void CelesteDiveKickState::exit() {
	controller->attack_fx.stop();
}

void CelesteDiveKickState::_bounce(
		Node3D *p_target,
		const Vector3 &p_dir
) {
	if (EnemyBase *eb = Object::cast_to<EnemyBase>(p_target)) {
		eb->take_damage(DIVE_DAMAGE);
	}
	controller->attack_fx.burst(p_target->get_global_position() + Vector3(0, 0.5f, 0));

	Vector3 back(-p_dir.x, 0.0f, -p_dir.z);
	if (back.length_squared() > 1e-4f) {
		back = back.normalized() * BOUNCE_BACK;
	}
	controller->set_velocity(back + Vector3(0.0f, controller->_jump_velocity0 * BOUNCE_UP, 0.0f));
	controller->can_air_spin = true; // reward: chain into another dive / spin
	controller->can_double_jump = true;
	controller->can_dash = true;
	controller->change_state(controller->jump_state); // rising: hover off, jump sound plays
}

void CelesteDiveKickState::physics_update(float delta) {
	time += delta;
	Node3D *target = _target();
	if (!target) {
		controller->change_state(controller->fall_state);
		return;
	}

	Vector3 to = target->get_global_position() - controller->get_global_position();
	float dist = to.length();
	if (Vector2(to.x, to.z).length() > 0.05f) {
		controller->set_rotation(Vector3(0.0f, Math::atan2(-to.x, -to.z), 0.0f)); // face the victim
	}

	if (time < DIVE_WINDUP) {
		float w = time / DIVE_WINDUP;
		controller->set_velocity(Vector3(0.0f, DIVE_HANG_VY * (1.0f - w), 0.0f));
		controller->attack_fx.update_dive(WINDUP_LEAN * w, 0.0f);
		return;
	}

	// Contact: close enough, or we bumped into it during last frame's move_and_slide.
	bool touched = dist <= DIVE_HIT_DIST;
	for (int i = 0; !touched && i < controller->get_slide_collision_count(); i++) {
		Ref<KinematicCollision3D> c = controller->get_slide_collision(i);
		touched = c.is_valid() && c->get_collider() == target;
	}
	Vector3 dir = (dist > 1e-4f) ? to / dist : Vector3(0, -1, 0);
	if (touched) {
		_bounce(target, dir);
		return;
	}

	controller->set_velocity(dir * DIVE_SPEED);
	// Feet first: lean back until the feet point along the dive (the body faces the target,
	// so in body space the dive is (0, dir.y, -horizontal)). Straight down = upright stomp,
	// flat = flying dropkick.
	float lean = Math::atan2(Vector2(dir.x, dir.z).length(), MAX(-dir.y, 0.0f));
	controller->attack_fx.update_dive(CLAMP(lean, KICK_LEAN_MIN, KICK_LEAN_MAX), 1.0f);

	bool landed = controller->is_on_floor() && time > DIVE_WINDUP + 0.05f;
	if (landed || time > DIVE_MAX_TIME) {
		if (landed) {
			controller->attack_fx.burst(controller->get_global_position() - Vector3(0, controller->half_height, 0));
		}
		controller->change_state(controller->fall_state);
	}
}

} // namespace godot
