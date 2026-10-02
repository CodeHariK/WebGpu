#ifndef CHARACTER_AUDIO_H
#define CHARACTER_AUDIO_H

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>

#include <vector>

namespace godot {

/**
 * CharacterAudio — one-shot footstep / jump / land sounds for the on-foot character.
 * ---------------------------------------------------------------------------------
 * Same modular contract as CharacterAnimator: it consumes the controller's locomotion
 * FACTS (grounded, vertical speed, horizontal speed, "jumped this frame") and decides
 * what to play. The controller never names a sound file.
 *
 *   - jump : fired on the frame a jump launches (ground, wall or air jump alike).
 *   - land : fired on the airborne -> grounded edge, louder the harder you hit.
 *   - step : while grounded and moving, on a cadence that tightens with speed, picking
 *            a random variation with a little pitch jitter so it never loops obviously.
 *
 * Players are positional (AudioStreamPlayer3D) children of the character so the sounds
 * pan with the body. Falloff is disabled like the car: the player's own character stays
 * audible wherever the camera is.
 */
class CharacterAudio {
private:
	AudioStreamPlayer3D *jump = nullptr;
	AudioStreamPlayer3D *land = nullptr;
	std::vector<AudioStreamPlayer3D *> steps; ///< One player per footstep variation.

	bool was_grounded = true;
	float fall_speed_peak = 0.0f; ///< Fastest downward speed during the current airtime (for land volume).
	float step_timer = 0.0f; ///< Seconds until the next footstep.
	int last_step = -1; ///< Avoid repeating the same variation twice in a row.

	// --- Feel tunables ---
	float step_speed_min = 1.0f; ///< Below this horizontal speed: no footsteps.
	float stride_length = 1.6f; ///< Metres of travel per footstep (interval = stride / speed).
	float step_interval_min = 0.22f; ///< Fastest footstep cadence (seconds).
	float land_speed_quiet = 3.0f; ///< Landing below this fall speed is silent.
	float land_speed_loud = 20.0f; ///< Landing at/above this is full volume.

	AudioStreamPlayer3D *_make_oneshot(
			Node *p_parent,
			const String &p_path
	);

public:
	/// Create the players as children of `p_parent`, loading from `p_dir` (res:// folder).
	void setup(
			Node *p_parent,
			const String &p_dir
	);
	bool is_ready() const { return jump != nullptr; }

	/// Call once per physics frame with the body's current facts.
	void update(
			bool p_grounded,
			float p_vy,
			float p_h_speed,
			bool p_jumped,
			float p_delta
	);
};

} // namespace godot

#endif // CHARACTER_AUDIO_H
