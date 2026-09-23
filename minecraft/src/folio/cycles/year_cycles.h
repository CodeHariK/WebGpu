#ifndef FOLIO_YEAR_CYCLES_H
#define FOLIO_YEAR_CYCLES_H

#include "cycle.h"

#include <godot_cpp/classes/node.hpp>

namespace godot {

/**
 * Folio port — FolioYearCycles  (folio `Game/Cycles/YearCycles.js`)
 * -----------------------------------------------
 * The annual/seasonal baseline. A FolioCycle interpolates four season presets
 * (winter / spring / summer / fall) over a full-year loop, producing the current
 * `leaves` / `temperature` / `humidity` / `clouds` / `wind` values. It pushes the
 * temperature + humidity into FolioWeather (retiring its `base_temperature` /
 * `base_humidity` stand-ins), so weather drifts with the season; FolioLeaves pulls
 * its density (`amount`) from the `leaves` track (autumn → full, spring → bare).
 *
 * Ticks at order 7 — BEFORE Weather (8) — so weather reads the fresh baseline the
 * same frame.
 *
 * Period: folio uses a real year (VITE_YEAR_CYCLE_PROGRESS forces a phase for a
 * build). Here the default is a short, game-friendly loop so seasons actually
 * cycle in play; `set_duration()` / `set_progress_override()` tune or lock it.
 *
 * Deferred vs folio: seasonal `clouds` / `wind` don't feed Weather yet (it has no
 * baseline hook for them — they stay pure noise); the `Overlay` sibling.
 */
class FolioYearCycles : public Node {
	GDCLASS(FolioYearCycles,
			Node)

private:
	FolioCycle cycle;
	bool enabled = true;
	double winter_temperature = -7.0; // winter keyframe (deg C); tunable at runtime

	void _build_keyframes();
	void _push();
	void _register_globals();
	bool tint_registered = false;

protected:
	static void _bind_methods();

public:
	FolioYearCycles();
	~FolioYearCycles();

	void _ready() override;

	void update(); // tick 7

	// Jump to a season NOW and keep the year advancing from there (0=winter start,
	// 0.125=winter, 0.375=spring, 0.625=summer, 0.875=fall). Unlike an override, it
	// does not freeze the cycle.
	void seek_season(double p_phase);

	// Runtime tuning of the winter temperature keyframe (colder = snowier). Rebuilds
	// the cycle in place, preserving the current year duration and phase.
	void set_winter_temperature(double p_celsius);
	double get_winter_temperature() const { return winter_temperature; }

	// Current interpolated seasonal values.
	double get_leaves() const { return cycle.get_float("leaves"); }
	double get_temperature() const { return cycle.get_float("temperature"); }
	double get_humidity() const { return cycle.get_float("humidity"); }
	double get_clouds() const { return cycle.get_float("clouds"); }
	double get_wind() const { return cycle.get_float("wind"); }

	// Lock the cycle to a fixed phase (0..1); pass < 0 to resume real time.
	void set_progress_override(double p_progress) { cycle.set_forced_progress(p_progress); }
	void set_duration(double p_seconds) { cycle.set_duration(p_seconds); }
	double get_progress() const { return cycle.get_progress(); }
};

} // namespace godot

#endif // FOLIO_YEAR_CYCLES_H
