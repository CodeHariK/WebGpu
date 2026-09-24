#ifndef FOLIO_LEAVES_H
#define FOLIO_LEAVES_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/**
 * Folio port — FolioLeaves  (folio `World/Leaves.js`)
 * ---------------------------------------------------
 * A field of autumn leaves drifting down around the camera, all in one mesh,
 * animated in the vertex shader (leaves.gdshader). This node owns the mesh +
 * material and, each tick (order 10), follows the view: it pushes the field
 * `size`/`center` from the camera's optimal area so the leaves always surround
 * the player. Time + wind come from the global shader clock / wind field, so
 * there is no per-frame time upload.
 *
 * Folio drives this as a GPU-compute particle sim (position/velocity buffers,
 * vehicle push, explosion, terrain-aware damping, YearCycles density). We port
 * the LOOK the same pragmatic way as RainLines: a procedural MultiMesh-style
 * field of tumbling quads. Deferred: vehicle push, explosion, terrain follow,
 * and YearCycles density (a plain `amount` gate stands in until YearCycles lands).
 */
class FolioLeaves : public Node3D {
	GDCLASS(FolioLeaves,
			Node3D)

private:
	int count = 1024; // folio 2^7..2^11; a middle default
	double elevation = 8.0;
	double amount = 0.6; // fraction of leaves shown (YearCycles stand-in)

	MeshInstance3D *mesh_instance = nullptr;
	Ref<ArrayMesh> mesh;
	Ref<ShaderMaterial> material;
	bool ready_done = false;

	Ref<ArrayMesh> _build_mesh() const;

protected:
	static void _bind_methods();

public:
	FolioLeaves();
	~FolioLeaves();

	void _ready() override;
	void update(); // tick 10

	void set_count(int p_n) { count = p_n < 1 ? 1 : p_n; }
	int get_count() const { return count; }

	void set_amount(double p_a);
	double get_amount() const { return amount; }

	void set_elevation(double p_e) { elevation = p_e; }
	double get_elevation() const { return elevation; }
};

} // namespace godot

#endif // FOLIO_LEAVES_H
