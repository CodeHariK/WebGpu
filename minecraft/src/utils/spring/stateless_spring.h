#ifndef STATELESS_SPRING_H
#define STATELESS_SPRING_H

#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * Stateless PD (proportional-derivative) spring-damper *force* helpers.
 *
 * One formula, `strength * displacement - damper * velocity`, produces a force
 * (or torque) you feed straight into the physics engine each frame
 * (RigidBody3D::apply_central_force / apply_torque). Used by SpringCharacter's
 * ride spring (float) and its upright/facing spring, and reusable anywhere a
 * critically-dampable pull-toward-a-goal force is needed (suspension, wobble).
 *
 * NOTE: this is distinct from `spring/stateful_spring.h`'s `SpringDynamics<T>`,
 * which is a *stateful integrator* that stores and smooths a value (cameras,
 * doors). This one computes a force and keeps no state.
 *
 * Tuning: for near-critical damping pick `damper ~= 2 * sqrt(strength)` (mass = 1);
 * lower is springy/bouncy, higher is sluggish.
 */
struct StatelessSpring {
	/// Scalar PD. `displacement` is (current - rest) or any error term; the sign
	/// convention is the caller's. Returns strength*displacement - damper*velocity.
	static inline float pd(float p_displacement, float p_velocity, float p_strength, float p_damper) {
		return (p_displacement * p_strength) - (p_velocity * p_damper);
	}

	/// Vector PD pulling `current` toward `goal` against `velocity`. Returns a force.
	static inline Vector3 pd_vec(
			const Vector3 &p_current,
			const Vector3 &p_goal,
			const Vector3 &p_velocity,
			float p_strength,
			float p_damper
	) {
		return ((p_goal - p_current) * p_strength) - (p_velocity * p_damper);
	}

	/// Angular PD: torque that rotates `current` toward `goal` (shortest arc) and
	/// damps `angular_velocity`. Feed straight into RigidBody3D::apply_torque().
	static inline Vector3 pd_torque(
			const Quaternion &p_current,
			const Quaternion &p_goal,
			const Vector3 &p_angular_velocity,
			float p_strength,
			float p_damper
	) {
		Quaternion to_goal = p_goal * p_current.inverse();
		if (to_goal.w < 0.0f) {
			// Take the shortest arc (q and -q are the same rotation).
			to_goal = Quaternion(-to_goal.x, -to_goal.y, -to_goal.z, -to_goal.w);
		}
		Vector3 axis = to_goal.get_axis();
		float angle = to_goal.get_angle();
		return (axis * (angle * p_strength)) - (p_angular_velocity * p_damper);
	}

	/// Cross-product "align axis to a goal axis" PD torque. `error = current x goal`
	/// has magnitude sin(angle) and lies perpendicular to both, so it corrects only
	/// the tilt between the two directions and leaves rotation ABOUT them (e.g. yaw,
	/// when aligning up-vectors) free. Cheaper than pd_torque and yaw-safe, but the
	/// restoring torque weakens past 90 deg and vanishes at 180 deg (unstable when
	/// fully inverted) -- fine for small-swing uprighting (hanging props), not for
	/// something that must recover from any orientation. Feed into apply_torque().
	static inline Vector3 pd_torque_align(
			const Vector3 &p_current,
			const Vector3 &p_goal,
			const Vector3 &p_angular_velocity,
			float p_strength,
			float p_damper
	) {
		Vector3 error = p_current.cross(p_goal);
		return (error * p_strength) - (p_angular_velocity * p_damper);
	}
};

} // namespace godot

#endif // STATELESS_SPRING_H
