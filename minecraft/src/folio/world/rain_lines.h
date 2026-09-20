#ifndef FOLIO_RAIN_LINES_H
#define FOLIO_RAIN_LINES_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/**
 * Folio port — FolioRainLines  (folio `World/RainLines.js`)
 * -----------------------------------------------
 * A tiled field of falling line-quads around the camera, all in one mesh, driven
 * entirely in the vertex shader (rain_lines.gdshader). This node owns the mesh +
 * material and, each tick (order 10), pushes the weather-driven uniforms:
 *   - visible_ratio = rain²                         (fraction of lines shown)
 *   - line_length   = lerp(remapClamp(rain,0,1,1,3), 0.03, snowRatio)
 *   - speed         = lerp(remapClamp(rain,0,1,0.2,0.4), 0.05, snowRatio)
 *   - incline       = remapClamp(wind,0,1,0.1,0.4)
 *   - center        = view optimal-area position (xz)
 *   - local_time   += deltaScaled · speed
 * with snowRatio = 1 - (1 - max(snow,0))⁴, so the same field becomes SNOW (short,
 * slow flecks) when it's cold. Hidden when visible_ratio ≈ 0.
 *
 * Deferred vs folio: the weatherRain achievement + the (commented) compute path.
 */
class FolioRainLines : public Node3D {
	GDCLASS(FolioRainLines,
			Node3D)

private:
	int count = 2048; // folio 2^11
	double speed = 0.25; // current computed fall speed
	double thickness = 0.015;
	double elevation = 20.0;

	MeshInstance3D *mesh_instance = nullptr;
	Ref<ArrayMesh> mesh;
	Ref<ShaderMaterial> material;
	double local_time = 0.0;
	bool ready_done = false;

	Ref<ArrayMesh> _build_mesh() const;

protected:
	static void _bind_methods();

public:
	FolioRainLines();
	~FolioRainLines();

	void _ready() override;
	void update(); // tick 10

	void set_count(int p_n) { count = p_n < 1 ? 1 : p_n; }
	int get_count() const { return count; }
};

} // namespace godot

#endif // FOLIO_RAIN_LINES_H
