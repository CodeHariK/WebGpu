#ifndef FOLIO_LIGHTING_H
#define FOLIO_LIGHTING_H

#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * Folio port — Lighting  (folio `Game/Ligthing.js` [sic], registered `FolioLighting`)
 * -----------------------------------------------------------------------------------
 * The sun: one directional light that follows the view's framed ground area, plus
 * the SHARED lighting uniforms the base material reads. Ticks at priority 9.
 *
 * Two outputs:
 *   1. A real `DirectionalLight3D` (drop/cast shadows) aimed from a spherical
 *      (phi, theta) direction at the view's optimal-area centre.
 *   2. Global shader uniforms consumed by the base material (folio's
 *      `game.lighting.*` in MeshDefaultMaterial):
 *        folio_light_direction / _color / _intensity   (sun)
 *        folio_light_bounce_edge_low/_high/_distance/_multiplier, folio_bounce_color
 *        folio_core_shadow_edge_low/_high               (wrap/core shading)
 *        folio_shadow_color                             (shadow tint)
 *
 * Deps not ported yet (hooks): DayCycles animates direction/color/intensity/
 * shadow color — `use_day_cycles` defaults false; feed via `set_day_progress()` +
 * `set_day_light_color/intensity/shadow_color()` when DayCycles lands. View
 * optimal-area radius/position and Quality are read via `FolioGame` if present,
 * else sensible defaults.
 *
 * Godot mapping note: Godot fits the directional shadow to the camera (PSSM), so
 * folio's manual ortho box maps approximately to shadow max-distance + bias/blur;
 * the shadow TINT and core shading live in the shader (the uniforms above), not on
 * the light.
 */
class FolioLighting : public Node3D {
	GDCLASS(FolioLighting,
			Node3D)

private:
	// Sun direction (spherical) + day-cycle sway.
	bool use_day_cycles = false;
	double phi = 0.63;
	double theta = 0.72;
	double phi_amplitude = 0.62;
	double theta_amplitude = 1.25;
	double day_progress = 0.0;
	Vector3 direction;

	// Sun colour / intensity (driven by day cycle when wired).
	Color light_color = Color(1, 1, 1);
	double light_intensity = 1.0;

	// Light-bounce (ambient fill from the ground) uniforms.
	double bounce_edge_low = -1.0;
	double bounce_edge_high = 1.0;
	double bounce_distance = 1.5;
	double bounce_multiplier = 1.0;
	Color bounce_color = Color(0.51f, 0.28f, 0.5f); // #82487f

	// Core (wrap) shading + shadow tint uniforms.
	double core_shadow_edge_low = -0.25;
	double core_shadow_edge_high = 1.0;
	Color shadow_color = Color(0.5f, 0.5f, 0.6f);

	// Shadow params (approximate mapping to Godot directional shadow).
	double near_plane = 1.0;
	double depth = 40.0;
	double shadow_bias = 0.03;
	double shadow_normal_bias = 1.0;
	double shadow_blur = 3.0;

	double radius = 20.0; // from view optimal area, else default

	DirectionalLight3D *light = nullptr;
	bool globals_registered = false;
	bool subscribed = false;

	void _register_globals();
	void _update_globals();
	void _update_shadow();
	bool _subscribe();
	void _refresh_from_view(); // pull optimal radius/position

protected:
	static void _bind_methods();

public:
	FolioLighting();
	~FolioLighting();

	void _ready() override;
	void update(); // tick 9

	// --- day-cycle hooks (wired when DayCycles lands) ---
	void set_use_day_cycles(bool p_on) { use_day_cycles = p_on; }
	void set_day_progress(double p_progress) { day_progress = p_progress; }
	void set_day_light_color(const Color &p_color) { light_color = p_color; }
	void set_day_light_intensity(double p_intensity) { light_intensity = p_intensity; }
	void set_day_shadow_color(const Color &p_color) { shadow_color = p_color; }

	Vector3 get_direction() const { return direction; }
};

} // namespace godot

#endif // FOLIO_LIGHTING_H
