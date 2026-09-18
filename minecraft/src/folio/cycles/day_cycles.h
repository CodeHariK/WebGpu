#ifndef FOLIO_DAY_CYCLES_H
#define FOLIO_DAY_CYCLES_H

#include "cycle.h"

#include <godot_cpp/classes/node.hpp>

namespace godot {

/**
 * Folio port — FolioDayCycles  (folio `Game/Cycles/DayCycles.js`)
 * -----------------------------------------------
 * Drives the day/night look. A FolioCycle interpolates the four folio presets
 * (day / dusk / night / dawn) over a 4-minute loop and pushes the current values
 * into the systems that already expose day-cycle hooks:
 *   FolioLighting : sun angle (day_progress) + light colour/intensity + shadow colour
 *   FolioFog      : fog colours A/B + near/far ratios
 *   FolioReveal   : reveal colour + intensity
 * Ticks at order 8 — BEFORE Lighting (9) and Fog/Reveal (10) — so they consume the
 * fresh values the same frame.
 *
 * Deferred vs folio: `electricField` / `temperature` tracks (no consumers yet), the
 * night/deepNight interval events, and the YearCycles sibling (its consumers —
 * foliage/weather — aren't ported yet).
 *
 * (Name kept as `FolioDayCycles` — no core Godot class collides.)
 */
class FolioDayCycles : public Node {
	GDCLASS(FolioDayCycles,
			Node)

private:
	FolioCycle cycle;
	bool enabled = true;

	void _build_keyframes();
	void _push();

protected:
	static void _bind_methods();

public:
	FolioDayCycles();
	~FolioDayCycles();

	void _ready() override;

	void update(); // tick 8

	// Lock the cycle to a fixed phase (0..1); pass < 0 to resume real time.
	void set_progress_override(double p_progress) { cycle.set_forced_progress(p_progress); }
	void set_duration(double p_seconds) { cycle.set_duration(p_seconds); }
	double get_progress() const { return cycle.get_progress(); }
};

} // namespace godot

#endif // FOLIO_DAY_CYCLES_H
