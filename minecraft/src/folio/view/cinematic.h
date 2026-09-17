#ifndef FOLIO_VIEW_CINEMATIC_H
#define FOLIO_VIEW_CINEMATIC_H

#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * Folio port — View / Cinematic  (from `Game/View.js` setCinematic + update)
 * --------------------------------------------------------------------------
 * Single responsibility: scripted camera moves. `start()` locks a target pose
 * (position + look target); as `progress` animates 0→1 the default camera blends
 * (lerp position, slerp orientation) toward that pose. `end()` releases it.
 *
 * Plain helper (no GDCLASS): folio's `this.cinematic = {}` sub-object.
 *
 * Deps not ported yet (left as hooks, mirroring folio's two gsap tweens):
 *   - `progress` is driven EXTERNALLY by a tween layer: to 1 over ~1.5s on start
 *     (power2.inOut), to 0 over ~1s on end.
 *   - `dof_target` is the depth-of-field strength the render layer should ease to
 *     (0 while cinematic, 1.5 otherwise). Read it from the DOF pass when ported.
 */
class ViewCinematic {
public:
	bool active = false;
	double progress = 0.0; // 0 = live camera, 1 = full cinematic pose (external tween)
	Vector3 position;
	Vector3 target;
	double non_ideal_ratio_offset = 10.0;
	double dof_target = 1.5; // DOF strength hook: 0 during cinematic, 1.5 normally

	// Begin a cinematic to `pos` looking at `tgt`. On non-ideal ratios the camera
	// is pushed back along its view vector so framing stays consistent.
	void
	start(const Vector3 &p_pos,
		  const Vector3 &p_tgt,
		  double p_ratio_overflow);
	void end();

	// Blend the given default-camera transform toward the cinematic pose by
	// `progress`. No-op while progress <= 0. Call each frame from View::update.
	void apply(Transform3D &r_default_cam) const;

	void set_progress(double p_progress) { progress = p_progress; }
	double get_progress() const { return progress; }
	bool is_active() const { return active; }
};

} // namespace godot

#endif // FOLIO_VIEW_CINEMATIC_H
