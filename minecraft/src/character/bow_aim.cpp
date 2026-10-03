#include "bow_aim.h"

#include "../camera/camera.h"
#include "../combat/projectile/projectile.h"
#include "../combat/projectile_launcher.h"
#include "../combat/trajectory_preview.h"

#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

static const float TAP_CHARGE = 0.15f; // a quick tap still fires a weak shot
static const Color PREVIEW_COLOR(1.0f, 0.95f, 0.6f, 0.9f);

void BowAim::setup(
		Node3D *p_owner,
		const char *p_profile_path
) {
	owner = p_owner;

	launcher = memnew(ProjectileLauncher);
	launcher->set_name("BowLauncher");
	launcher->set_position(muzzle);
	launcher->set_max_alive(6);
	ResourceLoader *rl = ResourceLoader::get_singleton();
	if (rl->exists(p_profile_path)) {
		launcher->set_profile(rl->load(p_profile_path));
	}
	owner->add_child(launcher);

	preview = memnew(TrajectoryPreview);
	preview->set_name("BowTrajectory");
	preview->set_color(PREVIEW_COLOR);
	owner->add_child(preview);
}

float BowAim::_gravity() const {
	Ref<ProjectileProfile> p = launcher ? launcher->get_profile() : Ref<ProjectileProfile>();
	return p.is_valid() ? p->get_ballistic_gravity() : 9.8f;
}

Vector3 BowAim::_velocity(GameCamera *p_cam) {
	float pitch = CLAMP(p_cam->get_pitch() + loft, min_pitch, max_pitch);
	aim_yaw = p_cam->get_yaw();
	float t = charge * (2.0f - charge); // ease-out: power comes quickly, tops off gently
	float speed = Math::lerp(min_speed, max_speed, t);
	Vector3 dir = Vector3(0, 0, -1).rotated(Vector3(1, 0, 0), pitch).rotated(Vector3(0, 1, 0), aim_yaw);
	return dir * speed;
}

bool BowAim::update(
		float p_dt,
		bool p_held,
		GameCamera *p_cam
) {
	if (!launcher || !preview || !p_cam) {
		return false;
	}
	if (p_held) {
		if (!aiming) {
			aiming = true;
			charge = TAP_CHARGE;
		}
		charge = MIN(1.0f, charge + p_dt / MAX(0.05f, draw_time));
		Vector3 vel = _velocity(p_cam);
		TypedArray<RID> exclude;
		if (CollisionObject3D *co = Object::cast_to<CollisionObject3D>(owner)) {
			exclude.push_back(co->get_rid());
		}
		preview->show_arc(launcher->get_global_position(), vel, _gravity(), p_dt, Projectile::get_hit_mask(), exclude);
		return false;
	}
	if (!aiming) {
		return false;
	}
	// Released: loose the arrow along exactly the previewed arc.
	Vector3 vel = _velocity(p_cam);
	launcher->fire_velocity(vel);
	cancel();
	return true;
}

void BowAim::cancel() {
	aiming = false;
	charge = 0.0f;
	if (preview) {
		preview->hide_arc();
	}
}

} // namespace godot
