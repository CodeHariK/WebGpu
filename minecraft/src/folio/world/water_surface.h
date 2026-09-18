#ifndef FOLIO_WATER_SURFACE_H
#define FOLIO_WATER_SURFACE_H

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/**
 * Folio port — FolioWaterSurface  (folio `World/WaterSurface.js`, visual slice)
 * -----------------------------------------------
 * A transparent water plane at the water elevation, rendered by
 * `material/shaders/folio/water_surface.gdshader`. Ported: ripples (default on) +
 * shore mask, lit through the shared folio pipeline (drop shadows + fog).
 * Deferred vs folio: ice + splashes + weather gating (need Weather), the quality-0
 * screen-blur refraction, ice physics, and the camera-follow recentring/resize.
 *
 * (Name kept as `FolioWaterSurface` — no core Godot class collides.)
 */
class FolioWaterSurface : public Node3D {
	GDCLASS(FolioWaterSurface,
			Node3D)

private:
	double plane_size = 400.0; // large; the shader pins it to the water elevation

	MeshInstance3D *mesh = nullptr;
	Ref<ShaderMaterial> material;

	void _build();

protected:
	static void _bind_methods();

public:
	FolioWaterSurface();
	~FolioWaterSurface();

	void _ready() override;
};

} // namespace godot

#endif // FOLIO_WATER_SURFACE_H
