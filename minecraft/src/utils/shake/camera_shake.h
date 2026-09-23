#ifndef CAMERA_SHAKE_H
#define CAMERA_SHAKE_H

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * @brief Trauma-based camera shake (Squirrel Eiserloh, GDC "Juicing Your Cameras").
 *
 * Events add to a `trauma` value in [0, 1]; the actual shake magnitude is
 * `trauma^exponent`, so shake ramps in fast and tails off softly. Trauma decays
 * linearly over time. Offsets are sampled from smooth Perlin noise (not
 * `random()`, which buzzes) on independent channels, so the motion looks like a
 * real hand-held shake rather than jitter.
 *
 * The result is an ADDITIVE rotation (pitch/yaw/roll) and local position offset
 * that the camera applies on top of its solved transform each frame — it never
 * feeds back into the follow springs.
 *
 * Usage: `init()` once, `add_trauma(amount)` on impacts (0.2 small, 0.5 medium,
 * 1.0 huge), `update(delta)` each frame, then read `rotation_offset()` /
 * `local_position_offset()`.
 */
struct CameraShake {
	float trauma = 0.0f; ///< Current trauma in [0, 1].
	float decay = 1.2f; ///< Trauma lost per second.
	float exponent = 2.0f; ///< shake = trauma^exponent (2 = quadratic ramp).

	// Maximum shake at full effect.
	float max_pitch = 0.05f; ///< Radians of pitch wobble.
	float max_yaw = 0.06f; ///< Radians of yaw wobble.
	float max_roll = 0.09f; ///< Radians of roll (banking) wobble.
	float max_offset = 0.20f; ///< Metres of positional shake (camera-local X/Y).

	float frequency = 24.0f; ///< Noise traversal speed (higher = buzzier).

	double time = 0.0; ///< Accumulated noise time.
	Ref<FastNoiseLite> noise; ///< Smooth noise source (Perlin).

	/// Create the noise source. Call once before use.
	void init() {
		noise.instantiate();
		noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
		noise->set_frequency(1.0f); // we scale time ourselves, so 1 unit = 1 sample
		noise->set_seed((int)UtilityFunctions::randi());
	}

	/// Add an impulse of trauma (clamped to 1).
	void add_trauma(float p_amount) {
		trauma = CLAMP(trauma + p_amount, 0.0f, 1.0f);
	}

	/// Advance time and bleed off trauma.
	void update(float p_delta) {
		if (p_delta <= 0.0f) {
			return;
		}
		time += (double)p_delta;
		trauma = MAX(0.0f, trauma - decay * p_delta);
	}

	/// True while there is any shake to apply.
	bool is_active() const { return trauma > 0.0001f; }

	/// Current shake magnitude in [0, 1].
	float amount() const { return Math::pow(trauma, exponent); }

	/// Sample noise channel `p_channel`, returns roughly [-1, 1].
	float _sample(int p_channel) const {
		if (noise.is_null()) {
			return 0.0f;
		}
		return noise->get_noise_2d((float)(time * frequency), (float)(p_channel * 137));
	}

	/// Additive Euler rotation offset (pitch, yaw, roll) in radians.
	Vector3 rotation_offset() const {
		float a = amount();
		return Vector3(max_pitch * a * _sample(0), max_yaw * a * _sample(1), max_roll * a * _sample(2));
	}

	/// Additive camera-local position offset (X right, Y up) in metres.
	Vector3 local_position_offset() const {
		float a = amount();
		return Vector3(max_offset * a * _sample(3), max_offset * a * _sample(4), 0.0f);
	}
};

} // namespace godot

#endif // CAMERA_SHAKE_H
