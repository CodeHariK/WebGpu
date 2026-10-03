#ifndef BOW_AIM_H
#define BOW_AIM_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class GameCamera;
class ProjectileLauncher;
class TrajectoryPreview;

/**
 * BowAim — hold R to draw, see the arc, release to loose an arrow. Body-agnostic helper.
 * -----------------------------------------------------------------------------------
 * Owned by a character (like CharacterControls). Each tick: update(dt, held, camera).
 *   - Hold: `charge` fills over `draw_time`; launch speed eases from min_speed to max_speed.
 *     Direction = camera yaw, pitch = camera pitch + `loft` (so a level camera still lobs),
 *     clamped. The TrajectoryPreview draws that exact arc every frame.
 *   - Release: fires through a ProjectileLauncher with the same velocity, and the arrow
 *     uses the same gravity (profile->get_ballistic_gravity()), so it follows the dots.
 * The owner faces get_aim_yaw() while is_aiming().
 */
class BowAim {
public:
	// --- Tunables ---
	float min_speed = 10.0f; ///< Tap shot (m/s).
	float max_speed = 24.0f; ///< Full draw (m/s).
	float draw_time = 0.6f; ///< Seconds to full draw.
	float loft = 0.45f; ///< Added to camera pitch (rad, ~26 deg): aim "a bit up" by default.
	float min_pitch = -0.35f; ///< Lowest aim (rad).
	float max_pitch = 1.2f; ///< Highest aim (rad).
	Vector3 muzzle = Vector3(0.25f, 0.35f, -0.55f); ///< Bow position in the owner's local space.

	/// Creates the launcher (with the arrow profile at `p_profile_path`) and the preview.
	void setup(
			Node3D *p_owner,
			const char *p_profile_path
	);
	/// Returns true on the tick an arrow is loosed.
	bool update(
			float p_dt,
			bool p_held,
			GameCamera *p_cam
	);
	/// Drop the draw without firing (e.g. the owner stopped being controlled).
	void cancel();

	bool is_aiming() const { return aiming; }
	float get_charge() const { return charge; }
	float get_aim_yaw() const { return aim_yaw; }

private:
	Node3D *owner = nullptr;
	ProjectileLauncher *launcher = nullptr;
	TrajectoryPreview *preview = nullptr;
	bool aiming = false;
	float charge = 0.0f;
	float aim_yaw = 0.0f;

	Vector3 _velocity(GameCamera *p_cam);
	float _gravity() const;
};

} // namespace godot

#endif // BOW_AIM_H
