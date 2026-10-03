#ifndef HELICOPTER_ENEMY_H
#define HELICOPTER_ENEMY_H

#include "../flying_enemy.h"

namespace godot {

/**
 * HelicopterEnemy — a chubby toy chopper with angry eyebrows. Circles you and fires missiles.
 * ------------------------------------------------------------------------------------------
 *   Idle:   lazy circle around where it was placed.
 *   Engage: orbits the player at `orbit_radius`, `altitude` up, facing its direction of travel
 *           (banking into the circle), and every `fire_interval` turns its nose toward you and
 *           fires a homing missile (default profile: res://assets/projectiles/heli_missile.tres).
 *           It slows down for a beat while firing, which is the opening for a counter-attack.
 *   Look:   spinning main + tail rotors, skids, round windscreen with eyes.
 */
class HelicopterEnemy : public FlyingEnemy {
	GDCLASS(HelicopterEnemy,
			FlyingEnemy)

private:
	float orbit_radius = 13.0f;
	float orbit_angle = 0.0f;
	float aim_hold = 0.0f; ///< >0: slowed down, nose on the target, about to fire
	bool fired_this_aim = false;
	Node3D *main_rotor = nullptr;
	Node3D *tail_rotor = nullptr;

protected:
	static void _bind_methods();

	void _build_model() override;
	void _think(
			float p_dt,
			Node3D *p_target
	) override;
	void _animate(float p_dt) override;
	String _default_profile_path() const override { return "res://assets/projectiles/heli_missile.tres"; }

public:
	HelicopterEnemy();

	void set_orbit_radius(float p_v) { orbit_radius = p_v; }
	float get_orbit_radius() const { return orbit_radius; }
};

} // namespace godot

#endif // HELICOPTER_ENEMY_H
