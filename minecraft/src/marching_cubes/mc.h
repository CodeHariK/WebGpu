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
	String source_mesh; // Which base GLB mesh this came from
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

class MCNode : public Node3D {
	GDCLASS(MCNode, Node3D)

private:
	String mesh_library_path = "res://assets/MarchingCubes.glb";
	bool use_full_library = false;
	HashMap<String, MeshConfig> mesh_library;

	std::vector<String> base_mesh_order;


protected:
	static void _bind_methods();

public:
	MCNode();
	~MCNode();

	void _ready() override;

	void set_mesh_library_path(const String &p_path);
	String get_mesh_library_path() const;

	void set_use_full_library(bool p_use);
	bool get_use_full_library() const;



	void load_mesh_library();
	void display_library();
	void generate_variants();
	void validate_full_library();
	void print_library_hashes() const;

	MeshConfig get_mesh_config(uint8_t p_hash) const;
	Dictionary get_variant_counts() const;
	std::vector<String> get_base_mesh_order() const { return base_mesh_order; }

	// Enum-driven generation
	void apply_transform_sequence(uint8_t p_base_hash, uint8_t p_variant_hash, const Transform3D &p_base_t, const std::vector<MCTransform> &p_sequence, const String &p_name);
	void apply_4_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_6_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_8_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_12_rotations(uint8_t p_base_hash, uint8_t p_variant_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_12_mirror_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_rotations(uint8_t p_base_hash, uint8_t p_variant_hash, const Transform3D &p_base_t, Vector3::Axis p_axis, const String &p_prefix, bool p_include_base);
	void apply_24_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);

	// Hash transformations
	static void apply_mct_transform(uint8_t &p_hash, Transform3D &p_transform, MCTransform p_op);
	static uint8_t rotate_y(uint8_t p_hash);
	static uint8_t rotate_x(uint8_t p_hash);
	static uint8_t rotate_z(uint8_t p_hash);
	static uint8_t scale_x(uint8_t p_hash);

	static String hash_to_binary(uint8_t p_hash);

	// Corner Helpers
	static const uint8_t C0 = 7;
	static const uint8_t C1 = 6;
	static const uint8_t C2 = 5;
	static const uint8_t C3 = 4;
	static const uint8_t C4 = 3;
	static const uint8_t C5 = 2;
	static const uint8_t C6 = 1;
	static const uint8_t C7 = 0;
};

} // namespace godot

#endif // MCNODE_H
