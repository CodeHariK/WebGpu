#ifndef JUMP_METRICS_H
#define JUMP_METRICS_H

namespace godot {

class Node;

/**
 * Closed-form jump metrics for the deterministic "Mario arc" both controllers use:
 * rise to `height` in `time_to_peak` (constant up-gravity), fall back to take-off height
 * in `time_to_descent` (stronger down-gravity), fall speed capped at `terminal_velocity`.
 *
 * One source of truth for level design: how far / how high a jump reaches with the
 * current (live-tuned) settings. Heights are relative to the take-off point (+ up).
 *
 *   airtime(dh)  seconds from take-off until landing at height dh (< 0 = can't get there)
 *   reach(dh)    horizontal distance covered by then at full run (or sprint) speed
 *   classify()   how hard a gap is: comfortable / precise / limit / sprint / double / out
 *
 * Double jump: the air jump REPLACES vertical speed (v0 * air_jump_mult), so after the
 * press the arc is fixed again; only the press time t1 is free. Pressing at the apex
 * gives the most height, pressing late in the fall the most distance (~2x a single
 * jump). double_airtime() takes the best press time by sampling t1.
 *
 * Air dash (once per airtime): flat and gravity-free for dash_time at dash_speed, then a
 * fall from rest. full_reach() takes the best chain of jump + double jump + dash, in either
 * order (double then dash, or dash then double), sampling both press times.
 */
struct JumpMetrics {
	float height = 4.0f; ///< apex height above take-off (m)
	float time_to_peak = 0.4f; ///< s
	float time_to_descent = 0.3f; ///< s, apex back down to take-off height
	float run_speed = 10.0f; ///< full-stick horizontal speed (m/s)
	float sprint_speed = 15.0f; ///< sprint horizontal speed (m/s)
	float terminal_velocity = 0.0f; ///< fall speed cap (m/s), 0 = none
	int air_jumps = 0; ///< extra jumps in the air (only the first is modelled)
	float air_jump_mult = 1.0f; ///< air jump launch speed as a fraction of the ground jump's
	bool air_dash = false; ///< one flat, gravity-free dash per airtime
	float dash_speed = 0.0f; ///< m/s
	float dash_time = 0.0f; ///< s
	float air_accel = 0.0f; ///< m/s^2 the air control brakes a dash back to run speed (0 = instantly)

	/// Fractions of the run reach that bound each difficulty band.
	static constexpr float COMFORT = 0.7f;
	static constexpr float PRECISE = 0.9f;

	enum Band {
		BAND_COMFORT, ///< <= 70% of run reach
		BAND_PRECISE, ///< <= 90%
		BAND_LIMIT, ///< <= 100%: possible at full run, no margin
		BAND_SPRINT, ///< only with sprint
		BAND_DOUBLE, ///< only with the double jump
		BAND_DASH, ///< only with the full jump + double jump + air dash chain
		BAND_OUT, ///< unreachable with a single jump
	};

	float launch_velocity() const;
	float rise_gravity() const;
	float fall_gravity() const;

	float airtime(float p_dh) const;
	float reach(
			float p_dh,
			bool p_sprint = false
	) const;
	/// Most airtime / distance with one air jump pressed at the best moment (single jump
	/// values if the character has no air jump).
	float double_airtime(float p_dh) const;
	float double_reach(
			float p_dh,
			bool p_sprint = false
	) const;
	/// Highest landing height reachable with the double jump (pressed at the apex).
	float double_apex() const;
	/// Farthest landing at height dh using everything: jump, double jump and air dash,
	/// best order and timing (falls back to double_reach without an air dash).
	float full_reach(
			float p_dh,
			bool p_sprint = false
	) const;
	/// Height (relative to take-off) t seconds after take-off.
	float height_at(float p_t) const;
	Band classify(
			float p_dx,
			float p_dh
	) const;

	/// Fills metrics from a supported controller (CelesteController, SpringCharacter).
	static bool from_node(
			Node *p_node,
			JumpMetrics &r_metrics
	);

private:
	float _fall_time(float p_drop) const; ///< time to fall p_drop metres from rest at the apex
	float _fall_dist(float p_t) const; ///< metres fallen from rest after p_t seconds
	float _arc_y(
			float p_v,
			float p_t
	) const; ///< height above launch, launched upward at p_v
	float _arc_land(
			float p_v,
			float p_y0,
			float p_dh
	) const; ///< time from a launch at p_v (height p_y0) to land at p_dh, < 0 = never
};

} // namespace godot

#endif // JUMP_METRICS_H
