#ifndef FOLIO_SNOW_H
#define FOLIO_SNOW_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/**
 * Folio port — FolioSnow  (folio `World/Snow.js`)
 * -----------------------------------------------
 * A camera-following snow sheet that accumulates as it snows and melts when it
 * warms. This node owns a subdivided ground plane + material and, each tick
 * (order 10):
 *   - derives a target snow `elevation` from the weather globals (folio formula:
 *     cold + wet accumulates, warm melts),
 *   - eases the accumulated `elevation` toward it (so snow builds up / thaws over
 *     time, not instantly),
 *   - maps that to `coverage` (0..1) which fades the sheet in and scales the lump
 *     height in the shader,
 *   - snaps the plane to a grid-rounded view position so it follows the camera
 *     without the world-locked lumps shimmering,
 *   - drifts the glitter sparkle.
 * Hidden while `elevation` is below the folio visibility gate (-0.9).
 *
 * Folio drives displacement from a render-to-texture elevation field with wheel
 * tracks + terrain/water integration; those are deferred. Here the lumps come
 * from perlin noise in the vertex shader (snow_ground.gdshader).
 */
class FolioSnow : public Node3D {
	GDCLASS(FolioSnow,
			Node3D)

private:
	int subdivisions = 96; // grid resolution (folio 256; lighter here)
	double size = 100.0; // world extent of the sheet (covers the view area)
	double surface_y = 0.03; // sits just above the floor

	// Accumulation state (folio `elevation`, clamped [-1, 0.5]).
	double elevation = -1.0;
	double accum_rate = 0.6; // how fast snow builds up / melts toward target
	bool first_update = true;

	double glitter_variation = 0.0;
	double glitter_time_mult = 0.05;

	MeshInstance3D *mesh_instance = nullptr;
	Ref<ArrayMesh> mesh;
	Ref<ShaderMaterial> material;
	bool ready_done = false;

	Ref<ArrayMesh> _build_mesh() const;

protected:
	static void _bind_methods();

public:
	FolioSnow();
	~FolioSnow();

	void _ready() override;
	void update(); // tick 10

	void set_size(double p_s) { size = p_s < 1.0 ? 1.0 : p_s; }
	double get_size() const { return size; }

	void set_subdivisions(int p_n) { subdivisions = p_n < 1 ? 1 : p_n; }
	int get_subdivisions() const { return subdivisions; }
};

} // namespace godot

#endif // FOLIO_SNOW_H
