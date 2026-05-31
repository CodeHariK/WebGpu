#ifndef MP_NODE_H
#define MP_NODE_H

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <vector>

namespace godot {

struct PrismMeshConfig {
	Ref<Mesh> mesh;
	Transform3D transform;
	uint8_t source_mesh = 0; // Tracks which base mesh generated this variant
};

class MPNode : public Node3D {
	GDCLASS(MPNode, Node3D)

private:
	String mesh_library_path;
	bool is_initialized = false;

	// 64 configurations for Marching Prisms (6 bits)
	PrismMeshConfig mesh_library[64];
	std::vector<uint8_t> base_mesh_order;

	// Bitwise helper for 120-degree Y rotation
	uint8_t rotate_y_120(uint8_t p_hash);

protected:
	static void _bind_methods();

public:
	MPNode();
	~MPNode();

	void _ready() override;

	void set_mesh_library_path(const String &p_path);
	String get_mesh_library_path() const;

	void load_mesh_library();
	void initialize_library();
	void display_library();

	// Core generator for the 64 states
	void generate_variants_by_120Y_rotation();
	void validate_full_library();

	// Helpers
	uint8_t binary_to_hash(const String &p_bin);
	String hash_to_binary(uint8_t p_hash);
	Dictionary get_variant_counts() const;
};

} // namespace godot

#endif // MP_NODE_H
