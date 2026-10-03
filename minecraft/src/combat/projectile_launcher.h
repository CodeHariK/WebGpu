#ifndef PROJECTILE_LAUNCHER_H
#define PROJECTILE_LAUNCHER_H

#include "projectile/projectile.h"
#include "projectile/projectile_profile.h"

#include <godot_cpp/classes/node3d.hpp>

#include <vector>

namespace godot {

/**
 * ProjectileLauncher — a muzzle that fires Projectiles. Attach to anything.
 * ------------------------------------------------------------------------------
 * Player, boss and turret all shoot through this: put it where the shot should come from,
 * give it a ProjectileProfile, call fire(target). The shot leaves toward the target with
 * the profile's `launch_lift` added (a little lob reads better than a laser), or along the
 * launcher's -Z when there's no target. `max_alive` caps how many of its shots exist at
 * once, so a boss can't flood the screen. The owner (this node's parent) is the shooter:
 * it can't hit itself, and it becomes the target if the shot is parried.
 */
class ProjectileLauncher : public Node3D {
	GDCLASS(ProjectileLauncher,
			Node3D)

private:
	Ref<ProjectileProfile> profile;
	int max_alive = 3;
	bool debug_draw = false;
	std::vector<uint64_t> alive; ///< Instance ids of shots still in flight.

	int _prune_alive();
	Projectile *_spawn();

protected:
	static void _bind_methods();

public:
	/// Fires one shot at `p_target` (may be null). Returns it, or null if capped.
	Projectile *fire(Node3D *p_target);
	/// Fires one free ballistic shot from here with an explicit velocity (aimed shots).
	Projectile *fire_velocity(const Vector3 &p_velocity);

	void set_profile(const Ref<ProjectileProfile> &p_profile) { profile = p_profile; }
	Ref<ProjectileProfile> get_profile() const { return profile; }
	void set_max_alive(int p_max) { max_alive = p_max; }
	int get_max_alive() const { return max_alive; }
	void set_debug_draw(bool p_on) { debug_draw = p_on; }
	bool get_debug_draw() const { return debug_draw; }
	int get_alive_count() { return _prune_alive(); }
};

} // namespace godot

#endif // PROJECTILE_LAUNCHER_H
