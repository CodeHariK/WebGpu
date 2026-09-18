#ifndef FOLIO_CYCLE_H
#define FOLIO_CYCLE_H

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

/**
 * Folio port — FolioCycle  (folio `Game/Cycles/Cycles.js`, interpolation core)
 * -----------------------------------------------
 * A time-driven keyframe interpolator (not a Node — a plain owned helper, like the
 * FolioView sub-parts). Named float and color tracks share one list of `stops`
 * (0..1). Each frame `update(elapsed)` computes `progress = fmod(elapsed/duration)`
 * (or a forced value), finds the two surrounding stops, and smoothstep-interpolates
 * every track. Seamless looping is handled by folio's "fake steps" trick applied
 * once in `finalize()`: a copy of the first stop is appended past 1.0 and a copy of
 * the last stop is prepended below 0.0, so `progress` always sits between two stops.
 *
 * Build: set_stops() then add_float_track()/add_color_track() (each parallel to the
 * stops), then finalize(). Read the current values with get_float()/get_color().
 *
 * Deferred vs folio: gsap override tweens and punctual/interval events
 * (night/deepNight) — add when a consumer needs them.
 */
class FolioCycle {
private:
	double duration = 240.0; // seconds per full loop (folio day = 4 min)
	double forced_progress = -1.0; // < 0 = derive from time; else locked
	double progress = 0.0;

	Vector<double> stops;
	HashMap<String, Vector<double>> float_tracks;
	HashMap<String, Vector<Color>> color_tracks;

	HashMap<String, double> float_values; // current interpolated
	HashMap<String, Color> color_values;

	bool finalized = false;

public:
	void set_duration(double p_seconds) { duration = p_seconds; }
	double get_duration() const { return duration; }

	// < 0 derives progress from time; 0..1 locks it (testing / inspector).
	void set_forced_progress(double p_progress) { forced_progress = p_progress; }
	double get_progress() const { return progress; }

	void set_stops(const Vector<double> &p_stops) { stops = p_stops; }
	void add_float_track(
			const String &p_name,
			const Vector<double> &p_values
	) {
		float_tracks[p_name] = p_values;
	}
	void add_color_track(
			const String &p_name,
			const Vector<Color> &p_values
	) {
		color_tracks[p_name] = p_values;
	}

	void finalize(); // add the wrap-around fake steps (once)
	void update(double p_elapsed_seconds); // recompute progress + all tracks

	double get_float(const String &p_name) const;
	Color get_color(const String &p_name) const;
};

} // namespace godot

#endif // FOLIO_CYCLE_H
