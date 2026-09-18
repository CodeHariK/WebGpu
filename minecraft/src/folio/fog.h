#ifndef FOLIO_FOG_H
#define FOLIO_FOG_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

/**
 * Folio port — Fog  (folio `Game/Fog.js`, registered `FolioFog`)
 * -------------------------------------------------------------
 * Owns the distance-fog + sky-gradient uniforms the base material reads
 * (MeshDefaultMaterial: `fog.strength.mix(outputColor, fog.color)`). Ticks at 10.
 *
 * Two coupled things in folio:
 *   - a screen-space RADIAL gradient (colorA → colorB by distance from a centre in
 *     viewport UV) used both as the scene background and as the fog COLOR;
 *   - a depth fog FACTOR = rangeFogFactor(near, far) that fades geometry into that
 *     colour with distance. near/far come from the View's optimal-area near/far
 *     distances, compressed by day-cycle ratios.
 *
 * Published global shader uniforms (consumed by the base .gdshader):
 *   folio_fog_color_a / _b        (COLOR)   gradient ends
 *   folio_fog_radial_center       (VEC2)    gradient centre in screen UV
 *   folio_fog_radial_start / _end (FLOAT)   gradient smoothstep edges
 *   folio_fog_near / _far         (FLOAT)   depth-fog range (world units)
 * The shader computes: mix = smoothstep(start,end, len(SCREEN_UV - center));
 * fogColor = mix(colorA,colorB,mix); fogFactor = smoothstep(near,far, viewDist).
 *
 * Deferred: rendering the actual background gradient (a WorldEnvironment/sky or
 * full-screen pass) — done when the scene/render tier lands; the uniforms here are
 * enough to build it. Day cycle drives colors + near/far ratios (hooks below).
 */
class FolioFog : public Node {
	GDCLASS(FolioFog,
			Node)

private:
	Color color_a = Color(0.75f, 0.85f, 0.95f);
	Color color_b = Color(0.55f, 0.70f, 0.90f);
	Vector2 radial_center = Vector2(0, 0);
	double radial_start = 0.0;
	double radial_end = 1.0;

	// Depth range, and the day-cycle ratios that compress it within [near,far].
	double near_distance = 5.0;
	double far_distance = 40.0;
	double fog_near_ratio = 0.0;
	double fog_far_ratio = 1.0;

	bool globals_registered = false;
	bool subscribed = false;

	void _register_globals();
	void _update_globals();
	bool _subscribe();

protected:
	static void _bind_methods();

public:
	FolioFog();
	~FolioFog();

	void _ready() override;
	void update(); // tick 10

	// --- day-cycle hooks ---
	void set_day_fog_colors(
			const Color &p_a,
			const Color &p_b
	);
	void set_day_fog_ratios(
			double p_near_ratio,
			double p_far_ratio
	);
};

} // namespace godot

#endif // FOLIO_FOG_H
