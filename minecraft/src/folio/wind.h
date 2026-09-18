#ifndef FOLIO_WIND_H
#define FOLIO_WIND_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

/**
 * Folio port — FolioWind  (folio `Game/Wind.js`)
 * -----------------------------------------------
 * Publishes a shared wind field that foliage shaders sample to sway. Direction is
 * a fixed vec2; `localTime` scrolls each frame (scaled by strength). The actual
 * per-position sway offset is computed in the consuming shaders (2-octave perlin,
 * folio `offsetNode`) from these globals + the shared perlin noise.
 *
 * Globals: folio_wind_direction (VEC2), folio_wind_position_frequency (FLOAT),
 *          folio_wind_strength (FLOAT), folio_wind_time (FLOAT).
 * Ticks at order 9. Weather modulation of strength is deferred (defaults 0.5).
 */
class FolioWind : public Node {
	GDCLASS(FolioWind,
			Node)

private:
	double angle = 0.0; // set to Math::PI * 0.6 in the constructor
	Vector2 direction;
	double position_frequency = 0.5;
	double strength = 0.5;
	double local_time = 0.0;
	double time_frequency = 0.1;

	bool globals_registered = false;
	void _register_globals();
	void _push();

protected:
	static void _bind_methods();

public:
	FolioWind();
	~FolioWind();

	void _ready() override;
	void update(); // tick 9

	void set_strength(double p_v) { strength = p_v; }
	double get_strength() const { return strength; }
};

} // namespace godot

#endif // FOLIO_WIND_H
