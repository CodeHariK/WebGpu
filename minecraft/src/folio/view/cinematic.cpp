#include "cinematic.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/quaternion.hpp>

namespace godot {

void FolioViewCinematic::start(
		const Vector3 &p_pos,
		const Vector3 &p_tgt,
		double p_ratio_overflow
) {
	active = true;
	position = p_pos;
	target = p_tgt;

	// Push the camera back along its view vector on non-ideal aspect ratios.
	if (p_ratio_overflow > 0.0) {
		const Vector3 delta = (position - target).normalized() * (p_ratio_overflow * non_ideal_ratio_offset);
		position += delta;
	}

	// Tween hooks (external): progress -> 1 over ~1.5s, DOF strength -> 0.
	dof_target = 0.0;
}

void FolioViewCinematic::end() {
	active = false;
	// Tween hooks (external): progress -> 0 over ~1s, DOF strength -> 1.5.
	dof_target = 1.5;
}

void FolioViewCinematic::apply(Transform3D &r_default_cam) const {
	if (progress <= 0.0) {
		return;
	}

	// Target pose: at `position`, looking at `target`.
	const Transform3D dummy = Transform3D(Basis(), position).looking_at(target, Vector3(0, 1, 0));

	// Blend position (lerp) and orientation (slerp) by progress.
	r_default_cam.origin = r_default_cam.origin.lerp(dummy.origin, progress);

	const Quaternion from_q = r_default_cam.basis.get_rotation_quaternion();
	const Quaternion to_q = dummy.basis.get_rotation_quaternion();
	r_default_cam.basis = Basis(from_q.slerp(to_q, progress));
}

} // namespace godot
