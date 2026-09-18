#ifndef FOLIO_REVEAL_H
#define FOLIO_REVEAL_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * Folio port — Reveal  (folio `Game/Reveal.js`, registered `FolioReveal`)
 * ----------------------------------------------------------------------
 * The intro "reveal" ring: the world draws only within a growing radius around a
 * centre, with a bright glowing ring at the leading edge. The base material reads
 * these uniforms (MeshDefaultMaterial):
 *   d = length(positionWorld.xz - center)
 *   if d > distance: discard
 *   mix = step(distance - thickness, d)        // 1 on the ring, 0 inside
 *   revealColor = color * intensity
 *   outColor = mix(outColor, revealColor, mix)
 *
 * Published globals: folio_reveal_center (VEC2), folio_reveal_distance,
 * folio_reveal_thickness, folio_reveal_intensity (FLOAT), folio_reveal_color (COLOR).
 *
 * Deferred: the intro step machine (updateStep 0/1/2 drives world.step, grid,
 * intro loader, inputs, audio, camera zoom — all un-ported). Here we own the
 * uniforms and expose `set_distance()` so an intro/tween can animate the reveal
 * (folio: 0 → 3.5 → 30 → 99999). Ticks at 10.
 *
 * Default `distance` = fully revealed, so with no intro the material shows
 * everything (nothing discarded).
 */
class FolioReveal : public Node {
	GDCLASS(FolioReveal,
			Node)

private:
	Vector2 center = Vector2(0, 0);
	double distance = 99999.0; // fully revealed by default (no intro yet)
	double thickness = 0.05;
	Color color = Color(0.91f, 0.56f, 1.0f); // #e88eff
	double intensity = 5.5;
	double intensity_multiplier = 1.0;

	bool globals_registered = false;
	bool subscribed = false;

	void _register_globals();
	void _update_globals();
	bool _subscribe();

protected:
	static void _bind_methods();

public:
	FolioReveal();
	~FolioReveal();

	void _ready() override;
	void update(); // tick 10

	// --- hooks (intro / day cycle) ---
	void set_center(const Vector3 &p_pos); // uses X/Z
	void set_distance(double p_distance) { distance = p_distance; }
	void set_thickness(double p_thickness) { thickness = p_thickness; }
	void set_intensity_multiplier(double p_mul) { intensity_multiplier = p_mul; }
	void set_day_reveal_color(const Color &p_color) { color = p_color; }
	void set_day_reveal_intensity(double p_intensity) { intensity = p_intensity; }

	double get_distance() const { return distance; }
};

} // namespace godot

#endif // FOLIO_REVEAL_H
