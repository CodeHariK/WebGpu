#ifndef FOLIO_WATER_H
#define FOLIO_WATER_H

#include <godot_cpp/classes/node.hpp>

namespace godot {

/**
 * Folio port — Water  (folio `Game/Water.js`, registered `FolioWater`)
 * -------------------------------------------------------------------
 * Just the water-line parameters. The base material whitens geometry near the
 * surface (foam line): `abs(positionWorld.y - surface_elevation) > thickness`
 * keeps the color, else paints it white.
 *
 * Published globals (static — no tick needed):
 *   folio_water_surface_elevation (FLOAT)
 *   folio_water_surface_thickness (FLOAT)
 *
 * `depth_elevation` is not a shader uniform here — it is the deeper water level
 * consumed by the water-surface mesh (ported later); exposed via getter.
 */
class FolioWater : public Node {
	GDCLASS(FolioWater,
			Node)

private:
	double surface_elevation = -0.3;
	double surface_thickness = 0.013;
	double depth_elevation = -1.5;

	bool globals_registered = false;
	void _register_globals();
	void _push();

protected:
	static void _bind_methods();

public:
	FolioWater();
	~FolioWater();

	void _ready() override;

	void set_surface_elevation(double p_value);
	void set_surface_thickness(double p_value);
	double get_surface_elevation() const { return surface_elevation; }
	double get_surface_thickness() const { return surface_thickness; }
	double get_depth_elevation() const { return depth_elevation; }
};

} // namespace godot

#endif // FOLIO_WATER_H
