#ifndef FOLIO_VIEW_VIEW_H
#define FOLIO_VIEW_VIEW_H

#include "cinematic.h"
#include "focus_point.h"
#include "optimal_area.h"
#include "roll.h"
#include "spherical.h"
#include "zoom.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node3d.hpp>

namespace godot {

/**
 * Folio port — View  (from `Game/View.js`, orchestrator only)
 * -----------------------------------------------------------
 * The camera rig. Owns the active Camera3D and the `mode`, composes the six
 * helper parts (spherical / roll / zoom / focus_point / optimal_area / cinematic)
 * and runs the per-frame sequence at tick priority 7:
 *   focus → zoom → spherical → position → look+roll → cinematic → camera → area.
 *
 * The registered node of the split — the helpers are plain sub-objects it owns
 * (mirrors folio's `this.spherical = {}` etc.).
 *
 * Standalone for now (no Game/Inputs/Quality): aspect comes from Godot's own
 * viewport; player-follow and quality are public hooks:
 *   - `set_target_position()` — the vehicle/player calls this each frame.
 *   - `set_quality_level()`   — feed from Quality when Game wires it up.
 * Deferred (need un-ported deps): free-fly mode, map controls, speed lines.
 */
class View : public Node3D {
	GDCLASS(View,
			Node3D)

public:
	enum Mode {
		MODE_DEFAULT = 1,
		MODE_FREE = 2,
	};

private:
	int mode = MODE_DEFAULT;
	int quality_level = 0;

	double ideal_ratio = 1920.0 / 1080.0;
	double ratio_overflow = 0.0;
	double fov_degrees = 25.0;

	Vector3 position;
	Vector3 previous_position;
	Vector3 view_delta;

	Camera3D *camera = nullptr;

	// Composed parts.
	ViewSpherical spherical;
	ViewRoll roll;
	ViewZoom zoom;
	ViewFocusPoint focus_point;
	ViewOptimalArea optimal_area;
	ViewCinematic cinematic;

	bool subscribed = false;
	bool _subscribe();
	double _aspect() const;

	void _on_window_resized();

protected:
	static void _bind_methods();

public:
	View();
	~View();

	void _ready() override;

	// Per-frame camera solve (subscribed to Ticker at priority 7).
	void update();

	// Aspect-driven recompute; heavier reframe deferred to next frame.
	void resize();

	// --- hooks ---
	void set_target_position(const Vector3 &p_pos); // player follow
	void set_quality_level(int p_level) { quality_level = p_level; }

	// Cinematic passthrough.
	void cinematic_start(
			const Vector3 &p_pos,
			const Vector3 &p_tgt
	);
	void cinematic_end();
	void set_cinematic_progress(double p_progress);

	// Roll kick (e.g. on landings / impacts).
	void roll_kick(double p_strength);

	Camera3D *get_camera() const { return camera; }
	Vector3 get_position() const { return position; }
	double get_optimal_radius() const { return optimal_area.radius; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::View::Mode);

#endif // FOLIO_VIEW_VIEW_H
