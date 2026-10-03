#ifndef PROJECTILE_PROFILE_H
#define PROJECTILE_PROFILE_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/color.hpp>

namespace godot {

/// Declares a float/bool/int/Color field with its setter and getter.
#define PROFILE_FIELD(m_type, m_name, m_default) \
private:                                         \
	m_type m_name = m_default;                   \
                                                 \
public:                                          \
	void set_##m_name(m_type p_value) {          \
		m_name = p_value;                        \
	}                                            \
	m_type get_##m_name() const {                \
		return m_name;                           \
	}

/**
 * ProjectileProfile — every tunable of a homing projectile, as a shareable Resource.
 * ---------------------------------------------------------------------------------
 * One Projectile class serves player arrows, boss missiles, turret darts and mortars; the
 * profile is what makes them different. Presets live in res://assets/projectiles/.
 *
 * Guidance:
 *   HOMING     steers toward the target (rules below). Launches steeply upward, gravity bends it
 *              over (`launch_gravity`), then homing dives it onto the target: a lob, not a laser.
 *   BALLISTIC  never steers. At launch it solves one arc to the target's predicted landing spot
 *              (horizontal speed = `speed`) and shows that spot as a filling danger zone; it
 *              blasts everything inside `blast_radius` on impact.
 *
 * Homing fairness levers (what makes it dodgeable / parryable):
 *   - speed below the player's run speed (10 m/s) -> it can be outrun.
 *   - turn_rate caps the turn: turn radius = speed / turn_rate, so a late sidestep wins.
 *   - commit_distance: homing switches off when this close, so a last-second dodge always works.
 *   - lose_lock_angle: once the target is this far off its nose it gives up and flies straight.
 *   - lead: 0 = chase the target, 1 = aim at the full predicted intercept.
 * Defaults are the "boss missile" preset.
 */
class ProjectileProfile : public Resource {
	GDCLASS(ProjectileProfile,
			Resource)

public:
	enum Guidance {
		GUIDANCE_HOMING = 0,
		GUIDANCE_BALLISTIC = 1,
	};
	enum WobbleStyle {
		WOBBLE_SHAKE = 0, ///< Nervous angle twitch (pitch/yaw/roll jitter). Menacing.
		WOBBLE_CORKSCREW = 1, ///< Model circles the flight path. Goofy, toy-rocket feel.
	};
	enum Style {
		STYLE_MISSILE = 0, ///< Chubby cartoon rocket: corkscrew wobble + smoke puffs.
		STYLE_ARROW = 1, ///< Thin arrow: decaying fishtail wiggle + fletching flutter.
	};

	// --- Flight ---
	PROFILE_FIELD(int, guidance, GUIDANCE_HOMING) ///< Guidance enum: steer, or one fixed arc.
	PROFILE_FIELD(float, speed, 7.0f) ///< Cruise speed (m/s). Ballistic: horizontal speed.
	PROFILE_FIELD(float, turn_rate, 100.0f) ///< Max steering turn (degrees/second).
	PROFILE_FIELD(float, lead, 0.5f) ///< Prediction amount 0..1 (0 = chase, 1 = intercept).
	PROFILE_FIELD(float, max_lead_time, 1.5f) ///< Cap on the predicted intercept time (s).
	PROFILE_FIELD(float, launch_time, 0.8f) ///< Seconds of unguided flight before homing starts.
	PROFILE_FIELD(float, launch_lift, 2.0f) ///< Upward tilt added to the launch direction (1 = 45 deg).
	PROFILE_FIELD(float, launch_gravity, 5.0f) ///< Gravity during the launch, bends the climb over (m/s^2).
	PROFILE_FIELD(float, commit_distance, 3.0f) ///< Homing stops inside this range (m).
	PROFILE_FIELD(float, lose_lock_angle, 110.0f) ///< Target this far off the nose -> lock lost (deg).
	PROFILE_FIELD(float, gravity, 0.0f) ///< Downward pull after launch (m/s^2). Ballistic: the arc's gravity.
	PROFILE_FIELD(float, min_flight_time, 0.9f) ///< Ballistic: shortest arc, so close shots still lob (s).
	PROFILE_FIELD(float, lifetime, 6.0f) ///< Seconds before it fizzles out on its own.

	// --- Hit / parry ---
	PROFILE_FIELD(float, hit_radius, 0.7f) ///< Distance to the target's centre that counts as a hit (m).
	PROFILE_FIELD(float, damage, 1.0f) ///< Passed to take_damage() on whatever it hits.
	PROFILE_FIELD(float, blast_radius, 0.0f) ///< >0: impact damages everything in this radius (m).
	PROFILE_FIELD(bool, parryable, true) ///< Can be reflected by a parrying target.
	PROFILE_FIELD(float, parry_radius, 1.8f) ///< Parry window: target parrying within this range (m).
	PROFILE_FIELD(float, parry_speed_scale, 1.6f) ///< Speed multiplier after being reflected.

	// --- Look ---
	PROFILE_FIELD(int, style, STYLE_MISSILE) ///< Style enum: which cartoon model + wobble.
	PROFILE_FIELD(int, wobble_style, WOBBLE_SHAKE) ///< Missile only: WobbleStyle enum.
	PROFILE_FIELD(float, wobble_amp, 0.12f) ///< Shake: angle (rad). Corkscrew: radius (m). Arrow: fishtail (rad).
	PROFILE_FIELD(float, wobble_freq, 6.0f) ///< Shake speed (base cycles per second).
	PROFILE_FIELD(float, model_scale, 1.0f) ///< Uniform scale of the cartoon model.
	PROFILE_FIELD(Color, body_color, Color(0.95f, 0.3f, 0.3f)) ///< Main tint.
	PROFILE_FIELD(bool, show_telegraph, true) ///< Ground reticle + predicted-aim marker.

	/// Gravity the arc actually uses (ballistic shots default to Earth-ish when left at 0).
	/// Aim previews must use this too, so the drawn arc matches the shot.
	float get_ballistic_gravity() const { return gravity > 0.0f ? gravity : 9.8f; }

protected:
	static void _bind_methods();
};

#undef PROFILE_FIELD

} // namespace godot

VARIANT_ENUM_CAST(ProjectileProfile::Guidance);
VARIANT_ENUM_CAST(ProjectileProfile::WobbleStyle);
VARIANT_ENUM_CAST(ProjectileProfile::Style);

#endif // PROJECTILE_PROFILE_H
