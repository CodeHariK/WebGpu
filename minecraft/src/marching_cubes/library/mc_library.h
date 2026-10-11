// MCNode: the tile library. Loads the base tile meshes from a .glb (each named by its 8-bit
// corner pattern, e.g. "00001111") and fills all 256 entries by rotating/mirroring them.
//   mc_library.cpp           loading + lookup
//   mc_library_variants.cpp  generating the 256 variants from the base meshes
//   mc_library_showcase.cpp  display_library(): every tile laid out in a grid, for checking
#ifndef MC_LIBRARY_H
#define MC_LIBRARY_H

#include "marching_cubes/core/mc_symmetry.h"

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
	Ref<Mesh> mesh;
	Transform3D transform;
	uint8_t source_mesh = 0; // Which base GLB mesh this came from
};

enum MCImportMode : uint8_t { IMPORT_21_BASEMESH, IMPORT_68_BASEMESH, IMPORT_COMPLETE };

class MCNode : public Node3D {
	GDCLASS(MCNode,
			Node3D)

private:
	String mesh_library_path = "res://assets/MarchingCubes.glb";
	MCImportMode import_mode = IMPORT_68_BASEMESH;
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

	void set_import_mode(MCImportMode p_mode);
	MCImportMode get_import_mode() const;

	void initialize_library();

	void load_mesh_library();
	void display_library();
	void generate_variants_by_21_basemesh();
	void generate_variants_by_4Y_rotation();
	void validate_full_library();

	MeshConfig get_mesh_config(uint8_t p_hash) const;
	Dictionary get_variant_counts() const;
	std::vector<uint8_t> get_base_mesh_order() const { return base_mesh_order; }

	// Enum-driven generation
	void apply_transform_sequence(
			uint8_t p_base_hash,
			uint8_t p_variant_hash,
			const Transform3D &p_base_t,
			const std::vector<MCTransform> &p_sequence,
			const String &p_name
	);
	void apply_6_rotations(
			uint8_t p_base_hash,
			const Transform3D &p_base_t,
			const String &p_prefix
	);
	void apply_8_rotations(
			uint8_t p_base_hash,
			const Transform3D &p_base_t,
			const String &p_prefix
	);
	void apply_12_rotations(
			uint8_t p_base_hash,
			uint8_t p_variant_hash,
			const Transform3D &p_base_t,
			const String &p_prefix
	);
	void apply_12_mirror_rotations(
			uint8_t p_base_hash,
			const Transform3D &p_base_t,
			const String &p_prefix
	);
	void apply_4_axis_rotations(
			uint8_t p_base_hash,
			uint8_t p_variant_hash,
			const Transform3D &p_base_t,
			Vector3::Axis p_axis,
			const String &p_prefix,
			bool p_include_base
	);
	void apply_24_rotations(
			uint8_t p_base_hash,
			const Transform3D &p_base_t,
			const String &p_prefix
	);
};

} // namespace godot

VARIANT_ENUM_CAST(godot::MCImportMode);

#endif // MC_LIBRARY_H
