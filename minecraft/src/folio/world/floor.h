#ifndef FOLIO_FLOOR_H
#define FOLIO_FLOOR_H

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/**
 * Folio port — FolioFloor  (folio `World/Floor.js`, visual slice)
 * -----------------------------------------------
 * A displaced ground plane that renders the island from the shared terrain data
 * map via `material/shaders/folio/mesh_floor.gdshader`. Visual-only: folio's
 * physics heightfield + bedrock (which need the physics system + player) are
 * deferred, as is the camera-follow recentring (static at origin for now).
 *
 * (Name kept as `FolioFloor` — no core Godot class collides.)
 */
class FolioFloor : public Node3D {
	GDCLASS(FolioFloor,
			Node3D)

private:
	double plane_size = 192.0; // matches FolioTerrain.size (world ±96)
	int subdivisions = 128; // displacement detail (folio ~ size / 1.5)

	MeshInstance3D *mesh = nullptr;
	Ref<ShaderMaterial> material;

	void _build();

protected:
	static void _bind_methods();

public:
	FolioFloor();
	~FolioFloor();

	void _ready() override;
};

} // namespace godot

#endif // FOLIO_FLOOR_H
