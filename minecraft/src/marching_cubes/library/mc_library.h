// MCNode: the tile library. Loads the tile meshes from a .glb (each named by its 8-bit corner
// pattern, e.g. "00001111"; extra variants end in "_v2", "_v3", ...) and fills the other entries
// by turning them about Y. Any tile set
// works (68 tiles, a heightmap set, a complete set): no mode setting is needed.
//   mc_library.cpp           loading + lookup
//   mc_library_variants.cpp  filling the gaps by Y turns, reporting what is still missing
//   mc_library_showcase.cpp  display_library(): every tile laid out in a grid, for checking
#ifndef MC_LIBRARY_H
#define MC_LIBRARY_H

#include <cstdint>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <vector>

namespace godot {
class Mesh;
class PackedScene;
class InputEvent;
class VBoxContainer;
class Panel;
class AcceptDialog;
class CUI;

struct MeshConfig {
	Ref<Mesh> mesh; // the first variant; null = no tile for this hash
	std::vector<Ref<Mesh>> variants; // every modelled variant, by number (variants[0] == mesh)
	Transform3D transform;
	uint8_t source_mesh = 0; // Which base GLB mesh this came from

	// The mesh a cell at p_cell (grid coordinates) uses: one of the variants, picked from the
	// cell position (see core/mc_variant.h).
	Ref<Mesh> mesh_for_cell(const Vector3i &p_cell) const;
};

class MCNode : public Node3D {
	GDCLASS(MCNode,
			Node3D)

private:
	String mesh_library_path = "res://assets/MarchingCubes.glb";
	bool is_initialized = false;
	MeshConfig mesh_library[256];

	std::vector<uint8_t> base_mesh_order;

protected:
	static void _bind_methods();

public:
	MCNode();
	~MCNode();

	void _ready() override;

	void set_mesh_library_path(const String &p_path);
	String get_mesh_library_path() const;

	void initialize_library();

	void load_mesh_library();
	void display_library();
	// Fill empty entries by turning each loaded tile about Y; returns how many were filled.
	int fill_y_rotations();
	// Print which drawable hashes still have no tile.
	void report_missing() const;

	MeshConfig get_mesh_config(uint8_t p_hash) const;
	Dictionary get_variant_counts() const;
	std::vector<uint8_t> get_base_mesh_order() const { return base_mesh_order; }
};

} // namespace godot

#endif // MC_LIBRARY_H
