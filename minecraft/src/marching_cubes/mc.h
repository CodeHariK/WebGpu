#ifndef MCNODE_H
#define MCNODE_H

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

enum MCTransform : uint8_t {
	RX90,
	RX180,
	RX270,
	RY90,
	RY180,
	RY270,
	RZ90,
	RZ180,
	RZ270,
	SX
};

enum MCImportMode : uint8_t {
	IMPORT_21_BASEMESH,
	IMPORT_68_BASEMESH,
	IMPORT_COMPLETE
};

class MCNode : public Node3D {
	GDCLASS(MCNode, Node3D)

private:
	String mesh_library_path = "res://assets/MarchingCubes.glb";
	MCImportMode import_mode = IMPORT_68_BASEMESH;
	bool is_initialized = false;
	MeshConfig mesh_library[256];

	std::vector<uint8_t> base_mesh_order;

	static uint8_t binary_to_hash(const String &p_bin);

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
	void apply_transform_sequence(uint8_t p_base_hash, uint8_t p_variant_hash, const Transform3D &p_base_t, const std::vector<MCTransform> &p_sequence, const String &p_name);
	void apply_6_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_8_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_12_rotations(uint8_t p_base_hash, uint8_t p_variant_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_12_mirror_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_4_axis_rotations(uint8_t p_base_hash, uint8_t p_variant_hash, const Transform3D &p_base_t, Vector3::Axis p_axis, const String &p_prefix, bool p_include_base);
	void apply_24_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);

	// Hash transformations
	static void apply_mct_transform(uint8_t &p_hash, Transform3D &p_transform, MCTransform p_op);
	static uint8_t rotate_y(uint8_t p_hash);
	static uint8_t rotate_x(uint8_t p_hash);
	static uint8_t rotate_z(uint8_t p_hash);
	static uint8_t scale_x(uint8_t p_hash);

	static String hash_to_binary(uint8_t p_hash);

	// Corner Helpers (Synced: Ci = Bit i)
	static const uint8_t C0 = 0;
	static const uint8_t C1 = 1;
	static const uint8_t C2 = 2;
	static const uint8_t C3 = 3;
	static const uint8_t C4 = 4;
	static const uint8_t C5 = 5;
	static const uint8_t C6 = 6;
	static const uint8_t C7 = 7;
};

} // namespace godot

VARIANT_ENUM_CAST(godot::MCImportMode);

#endif // MCNODE_H
