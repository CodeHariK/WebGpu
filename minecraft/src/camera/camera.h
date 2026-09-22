#ifndef GAME_CAMERA_H
#define GAME_CAMERA_H

#include "../utils/raycast/mc_raycast.h"
#include "../utils/spring/spring_dynamics.h"
#include "camera_state.h"
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <memory>

namespace godot {
class PlayerInput;
class GameManager;

/**
 * GameCamera — the game's main 3D camera rig.
 * ------------------------------------------------------------------
 * A single Camera3D that switches between four behaviours (fly / car / TPS /
 * fixed) via a small state machine. The camera owns all the smoothing state and
 * shared follow maths; each `CameraState` only decides where the camera *wants*
 * to be this frame and then calls the shared helpers below to smooth toward it.
 *
 * Smoothing is done with second-order springs (`SpringDynamics`) so motion has
 * weight and settles without overshoot. Every spring is re-seated on the live
 * transform whenever a mode is entered (`rebase_springs()`), so switching modes
 * never snaps or inherits stale state.
 *
 * Lifecycle: on `_ready` it registers itself with the GameManager, resolves its
 * follow target, seats the springs and enters `camera_mode`. It drives itself
 * from `_physics_process`, delegating each frame to the active state.
 *
 * Everything a state needs is exposed to it through friendship rather than a
 * wide public surface; the public API is only what other gameplay systems (the
 * GameManager, player controllers, tools) actually call.
 */
class GameCamera : public Camera3D {
	GDCLASS(GameCamera,
			Camera3D)

	// The state machine and GameManager reach camera internals directly.
	friend class GameManager;
	friend class CameraState;
	friend class CameraStateFly;
	friend class CameraStateCar;
	friend class CameraStateTPS;
	friend class CameraStateFixed;

public:
	/// The available camera behaviours; selected via `set_camera_mode()`.
	enum Mode {
		MODE_FLY, ///< Free-flying debug/spectator camera (orbit, pan, dolly).
		MODE_CAR, ///< Arcade chase cam that trails a vehicle by its velocity.
		MODE_TPS, ///< Over-the-shoulder third-person cam with a captured mouse.
		MODE_FIXED ///< Top-down follow with optional dead-zone (Zelda-like).
	};

private:
	// --- State & targeting --------------------------------------------------
	Mode camera_mode = MODE_FLY; ///< Which behaviour is currently active.
	std::unique_ptr<CameraState> current_mode_instance; ///< The live behaviour object.

	NodePath follow_target_path; ///< Scene path to the node the camera follows.
	Node3D *follow_target_node = nullptr; ///< Resolved follow target (cached).
	/// Physics RIDs excluded from every camera ray (the follow target and its
	/// bodies). Rebuilt only when the target changes, not per frame.
	TypedArray<RID> follow_exclude;

	// --- Orientation --------------------------------------------------------
	float yaw = 0.0f; ///< Desired yaw (radians, around +Y); smoothed via yaw_spring.
	float pitch = 0.0f; ///< Desired pitch (radians, around +X); smoothed via pitch_spring.

	// --- Smoothing config ---------------------------------------------------
	bool pos_smoothing_enabled = true; ///< When false the camera snaps to its ideal position.

	float frequency = 2.0f; ///< Base spring natural frequency (Hz); higher = snappier.
	float damping = 1.0f; ///< Spring damping ratio (1 = critical, <1 springy, >1 sluggish).
	float response = 0.0f; ///< Spring anticipation/overshoot term (0 = none).

	/// Rotation and distance springs run faster than the position spring by
	/// these factors, replacing the magic 2.0 / 1.5 that used to be hard-coded
	/// in every state.
	float rotation_freq_mult = 2.0f; ///< Rotation spring frequency = frequency * this.
	float distance_freq_mult = 1.5f; ///< Distance spring frequency = frequency * this.

	// --- Smoothing state (one spring per smoothed quantity) -----------------
	SpringDynamics<Vector3> pos_spring; ///< Smooths the camera world position.
	SpringDynamics<float> dist_spring; ///< Smooths the follow distance (metres).
	SpringDynamics<float> yaw_spring; ///< Smooths yaw toward the `yaw` target.
	SpringDynamics<float> pitch_spring; ///< Smooths pitch toward the `pitch` target.

	// --- Orbit / interaction ------------------------------------------------
	Vector3 follow_offset = Vector3(0, 2, 8); ///< Resting offset from pivot; its dir = orbit dir, length = rest distance.
	float target_distance = 10.0f; ///< Desired distance from the pivot (metres).
	float min_distance = 2.0f; ///< Closest the camera may sit to the pivot.
	float max_distance = 50.0f; ///< Farthest the camera may sit from the pivot.
	float pan_speed = 0.05f; ///< Fly-mode pan speed (metres per input unit).
	float zoom_speed = 1.0f; ///< Zoom/dolly speed (metres per scroll unit).
	float orbit_sensitivity = 0.005f; ///< Radians of yaw/pitch per input unit.
	bool is_orbiting = false; ///< Currently unused flag (orbit state lives in PlayerInput).

	// --- Collision ----------------------------------------------------------
	bool collision_enabled = true; ///< When true, pull the camera in to avoid clipping geometry.
	uint32_t collision_mask = 1; ///< Physics layers the collision ray tests against.
	float collision_margin = 0.2f; ///< Gap kept between the camera and a hit surface.

	PlayerInput *player_input = nullptr; ///< Source of look/zoom/orbit input (not owned).

	// --- Dynamic zoom (car) -------------------------------------------------
	bool dynamic_zoom_enabled = true; ///< Pull the camera back as the vehicle speeds up.
	float speed_threshold = 10.0f; ///< Speed (m/s) above which dynamic zoom starts.
	float dynamic_zoom_extra_distance = 6.0f; ///< Extra distance added at max speed.
	float max_speed_for_zoom = 30.0f; ///< Speed (m/s) at which dynamic zoom is fully applied.

	// --- Per-type feel tunables ---------------------------------------------
	/// Car: lead the framing ahead of the vehicle along its travel direction
	/// (metres at max speed) so the player sees more of where they are going.
	float car_look_ahead = 2.5f;
	/// Car: fraction to flatten the pitch toward level at top speed, for a
	/// stronger sense of velocity (0 = never flatten).
	float car_speed_pitch_flatten = 0.12f;
	/// Fixed: ground-plane radius the target may drift before the camera starts
	/// following (0 = rigid follow, the original behaviour).
	float fixed_deadzone = 0.0f;

	/// Resolve `follow_target_path` into `follow_target_node` (no-op if unset).
	void _update_follow_node();

	/// Rebuild `follow_exclude` from the current follow target. Call whenever the
	/// follow target changes.
	void _refresh_follow_exclude();

	// --- Shared follow helpers (called by the follow states) ----------------

	/// Re-seat every spring on the current camera transform so entering a mode
	/// never snaps or carries stale state (e.g. a distance in the wrong units).
	void rebase_springs();

	/// Normalised direction of `follow_offset` (falls back to straight behind).
	Vector3 base_offset_dir() const;

	/// Step the yaw/pitch springs toward the `yaw`/`pitch` targets, taking the
	/// shortest angular path so a +PI/-PI wrap never spins the camera.
	void smooth_look_angles(float p_delta);

	/// World position on the orbit at `p_dist` from `p_pivot`, using the current
	/// *smoothed* yaw/pitch (so it matches what the camera is actually showing).
	Vector3 orbit_position(
			const Vector3 &p_pivot,
			float p_dist
	) const;

	/// Resolve the collision-limited follow distance and smooth it: snaps inward
	/// instantly to avoid clipping through walls, eases back out once clear.
	/// `p_ideal_full` is the un-occluded camera position used for the ray.
	/// Returns the distance to place the camera at this frame.
	float resolve_follow_distance(
			const Vector3 &p_pivot,
			const Vector3 &p_ideal_full,
			float p_desired,
			float p_delta
	);

	/// Smooth the camera toward `p_ideal_pos` (respecting `pos_smoothing_enabled`)
	/// and apply the smoothed position + smoothed yaw/pitch to the transform.
	void apply_position(
			const Vector3 &p_ideal_pos,
			float p_delta
	);

	/// Cast from `p_from` toward `p_to`; return the safe distance to the first
	/// hit (minus the margin, clamped to `min_distance`), or the full distance
	/// when nothing is hit. Excludes the follow target from the ray.
	float _solve_collision(
			const Vector3 &p_from,
			const Vector3 &p_to
	);

protected:
	/// Register properties, methods and the Mode enum with Godot.
	static void _bind_methods();

public:
	GameCamera();
	~GameCamera();

	/// Resolve the target, register with the GameManager, seat springs, enter mode.
	void _ready() override;
	/// Deregister from the GameManager if we were the active camera.
	void _exit_tree() override;
	/// Per-frame driver: delegates to the active state (physics tick).
	void _physics_process(double p_delta) override;

	/// Switch behaviour. Exits the old state, enters the new one; no-op if same.
	void set_camera_mode(Mode p_mode);
	Mode get_camera_mode() const;

	/// Current *desired* yaw/pitch in radians (pre-smoothing).
	float get_yaw() const { return yaw; }
	float get_pitch() const { return pitch; }

	/// The node the camera follows, by scene path.
	void set_follow_target_path(const NodePath &p_path);
	NodePath get_follow_target_path() const;

	/// The node the camera follows, by pointer (also updates the stored path).
	void set_follow_target_node(Node3D *p_node);
	Node3D *get_follow_target_node() const { return follow_target_node; }

	/// The input source the states read look/zoom/orbit from (not owned).
	void set_player_input(PlayerInput *p_input) { player_input = p_input; }
	PlayerInput *get_player_input() const { return player_input; }

	/// Base spring frequency (Hz) — how quickly the camera chases its target.
	void set_frequency(float p_freq) { frequency = p_freq; }
	float get_frequency() const { return frequency; }

	/// Spring damping ratio (1 = critical, <1 springy/bouncy, >1 sluggish).
	void set_damping(float p_damping) { damping = p_damping; }
	float get_damping() const { return damping; }

	/// Spring anticipation/overshoot toward a moving target (0 = none).
	void set_response(float p_response) { response = p_response; }
	float get_response() const { return response; }

	/// Fly-mode pan speed (metres per input unit).
	void set_pan_speed(float p_speed) { pan_speed = p_speed; }
	float get_pan_speed() const { return pan_speed; }

	/// Zoom/dolly speed (metres per scroll unit).
	void set_zoom_speed(float p_speed) { zoom_speed = p_speed; }
	float get_zoom_speed() const { return zoom_speed; }

	/// Look sensitivity (radians of yaw/pitch per input unit).
	void set_orbit_sensitivity(float p_sensitivity) { orbit_sensitivity = p_sensitivity; }
	float get_orbit_sensitivity() const { return orbit_sensitivity; }

	/// Resting offset from the pivot. Setting it also re-derives `target_distance`
	/// and grows `max_distance` if needed (see the .cpp).
	void set_follow_offset(const Vector3 &p_offset);
	Vector3 get_follow_offset() const { return follow_offset; }

	/// Distance clamp bounds (metres).
	void set_min_distance(float p_dist) { min_distance = p_dist; }
	float get_min_distance() const { return min_distance; }
	void set_max_distance(float p_dist) { max_distance = p_dist; }
	float get_max_distance() const { return max_distance; }

	/// Whether the camera pulls in to avoid clipping level geometry.
	void set_collision_enabled(bool p_enabled) { collision_enabled = p_enabled; }
	bool is_collision_enabled() const { return collision_enabled; }

	/// Whether the position is spring-smoothed (false = snap to ideal).
	void set_pos_smoothing_enabled(bool p_enabled) { pos_smoothing_enabled = p_enabled; }
	bool is_pos_smoothing_enabled() const { return pos_smoothing_enabled; }

	/// Cast a ray straight out of the camera's forward vector; returns the hit
	/// (used by gameplay for aim/interaction). Excludes the follow target.
	MCRaycastHit get_center_raycast_hit(
			uint32_t p_mask = 0xFFFFFFFF,
			float p_dist = 1000.0f
	);

	/// Distance the camera should sit at right now, including car dynamic zoom.
	float get_current_target_distance() const;

	/// Dynamic-zoom tuning (car mode): pull back with speed.
	void set_dynamic_zoom_enabled(bool p_enabled) { dynamic_zoom_enabled = p_enabled; }
	bool is_dynamic_zoom_enabled() const { return dynamic_zoom_enabled; }
	void set_speed_threshold(float p_threshold) { speed_threshold = p_threshold; }
	float get_speed_threshold() const { return speed_threshold; }
	void set_dynamic_zoom_extra_distance(float p_dist) { dynamic_zoom_extra_distance = p_dist; }
	float get_dynamic_zoom_extra_distance() const { return dynamic_zoom_extra_distance; }
	void set_max_speed_for_zoom(float p_speed) { max_speed_for_zoom = p_speed; }
	float get_max_speed_for_zoom() const { return max_speed_for_zoom; }

	/// Car: metres of velocity look-ahead applied at max speed (0 disables).
	void set_car_look_ahead(float p_v) { car_look_ahead = p_v; }
	float get_car_look_ahead() const { return car_look_ahead; }

	/// Fixed: ground-plane dead-zone radius before the camera follows (0 = rigid).
	void set_fixed_deadzone(float p_v) { fixed_deadzone = MAX(0.0f, p_v); }
	float get_fixed_deadzone() const { return fixed_deadzone; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::GameCamera::Mode);

#endif // GAME_CAMERA_H
