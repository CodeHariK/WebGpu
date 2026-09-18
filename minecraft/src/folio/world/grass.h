#ifndef FOLIO_GRASS_H
#define FOLIO_GRASS_H

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/**
 * Folio port — FolioGrass  (folio `World/Grass.js`, visual slice)
 * -----------------------------------------------
 * A GPU grass field: one mesh of subdiv² camera-facing blade triangles, built
 * once on the CPU (jittered grid of blade positions + per-vertex height
 * randomness), driven entirely in `grass.gdshader`. Follows the camera each tick
 * by pushing the framed area centre into the shader (toroidal wrap). Height and
 * visibility come from the terrain grass channel; tips sway with FolioWind.
 *
 * Deferred vs folio: viewport-resize regen + surfaceOverflow blade sizing, the
 * `shadowNode` (tip/base) term, and wheel-track flattening.
 */
class FolioGrass : public Node3D {
	GDCLASS(FolioGrass,
			Node3D)

private:
	int subdivisions = 200; // folio 280; blades = subdivisions²
	double size = 40.0; // world extent (folio: optimalArea.radius * 2)
	double blade_width = 0.1;
	double blade_height = 0.6;
	bool scale_with_quality = true; // low tier -> fewer blades

	MeshInstance3D *mesh = nullptr;
	Ref<ShaderMaterial> material;

	void _build();
	void _rebuild();

protected:
	static void _bind_methods();

public:
	FolioGrass();
	~FolioGrass();

	void _ready() override;
	void update(); // tick 10 — follow the framed area

	// Tunables (also exposed as node properties; changing after build rebuilds).
	void set_subdivisions(int p_v);
	int get_subdivisions() const { return subdivisions; }
	void set_field_size(double p_v);
	double get_field_size() const { return size; }
	void set_blade_width(double p_v);
	double get_blade_width() const { return blade_width; }
	void set_blade_height(double p_v);
	double get_blade_height() const { return blade_height; }
	void set_scale_with_quality(bool p_v) { scale_with_quality = p_v; }
	bool get_scale_with_quality() const { return scale_with_quality; }
};

} // namespace godot

#endif // FOLIO_GRASS_H
