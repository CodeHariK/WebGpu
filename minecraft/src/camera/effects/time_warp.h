#ifndef GAME_TIME_WARP_H
#define GAME_TIME_WARP_H

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

class ColorRect;

/**
 * TimeWarp — GTA-style bullet-time with an energy meter and low-time warning.
 * --------------------------------------------------------------------------
 * A CanvasLayer holding a full-rect ColorRect that runs `slowmo.gdshader`.
 * While active it eases `Engine.time_scale` down to `slow_scale` (physics and
 * every delta-driven system, including the camera springs, slow with it) and
 * fades in a desaturate + cool-tint grade. An `energy` meter drains in REAL
 * time while active and refills when off; at zero it auto-disengages. As the
 * meter drops below `warn_threshold` a pulsing red edge flash ramps up.
 *
 * All timing uses wall-clock delta (via Time ticks), not the scaled frame
 * delta, so the meter drains and the effect eases at a fixed real-world rate
 * regardless of how slow game time is running.
 *
 * Control it from gameplay: `set_active(true)` / `toggle()` on a button. The
 * red flash is also exposed via `set_warn()` so any "low X" state (a countdown,
 * low health) can drive the same warning independently of bullet-time.
 *
 * Restores `time_scale` to 1 on exit so the game is never left slowed.
 */
class TimeWarp : public CanvasLayer {
	GDCLASS(TimeWarp,
			CanvasLayer)

private:
	Ref<ShaderMaterial> material; ///< Owns the slowmo shader instance.
	ColorRect *rect = nullptr; ///< Full-rect overlay the grade draws on.
	int layer_index = 96; ///< CanvasLayer order (above speed lines, below UI).

	bool active = false; ///< Whether bullet-time is engaged.
	float energy = 1.0f; ///< Meter in [0, 1]; drains while active.

	// Tunables.
	float slow_scale = 0.35f; ///< Engine.time_scale while fully engaged.
	float engage_rate = 8.0f; ///< time_scale ease rate (e-folds/sec, real time).
	float grade_rate = 9.0f; ///< Grade fade rate (e-folds/sec).
	float warn_rate = 10.0f; ///< Warning ramp rate (e-folds/sec).
	float drain_rate = 0.40f; ///< Energy lost per real second while active.
	float refill_rate = 0.25f; ///< Energy gained per real second while off.
	float warn_threshold = 0.30f; ///< Energy below which the flash begins.
	float min_energy_to_start = 0.12f; ///< Won't engage below this (avoids stutter).

	// Smoothed shader inputs.
	float current_amount = 0.0f; ///< Eased grade strength.
	float current_warn = 0.0f; ///< Eased warning strength.
	float external_warn = 0.0f; ///< Caller-driven warning (max'd with the meter).

	uint64_t last_usec = 0; ///< Wall-clock tick of the previous frame.
	bool debug_force = false; ///< `--twshot`: force the effect on for a screenshot.
	bool debug_keys = false; ///< `--fxtest`: T=toggle slow-mo, G=drain, R=refill.

	void _build_overlay();
	float _real_delta();
	void _fx_capture();

protected:
	static void _bind_methods();

public:
	TimeWarp();
	~TimeWarp();

	void _ready() override;
	void _process(double p_delta) override;
	void _exit_tree() override;
	/// Debug test keys when launched with `--fxtest`.
	void _unhandled_key_input(const Ref<InputEvent> &p_event) override;

	/// Engage / disengage bullet-time (engages only if there is enough energy).
	void set_active(bool p_active);
	bool is_active() const { return active; }
	void toggle();

	/// Current meter level in [0, 1].
	float get_energy() const { return energy; }
	void set_energy(float p_e);

	/// Drive the red low-time flash directly (0..1), for any "low X" warning.
	void set_warn(float p_warn);

	void set_slow_scale(float p_v) { slow_scale = p_v; }
	float get_slow_scale() const { return slow_scale; }
	void set_drain_rate(float p_v) { drain_rate = p_v; }
	float get_drain_rate() const { return drain_rate; }
	void set_refill_rate(float p_v) { refill_rate = p_v; }
	float get_refill_rate() const { return refill_rate; }
	void set_warn_threshold(float p_v) { warn_threshold = p_v; }
	float get_warn_threshold() const { return warn_threshold; }
};

} // namespace godot

#endif // GAME_TIME_WARP_H
