#ifndef MCNODE_H
#define MCNODE_H

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <vector>

#include "cui/cui.h"

namespace godot {

struct MeshConfig {
	Ref<Mesh> mesh;
	Transform3D transform;
	String source_mesh; // Which base GLB mesh this came from
};

enum MCTransform {
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
	GDCLASS(MCNode, Node3D);

private:
	String mesh_library_path = "res://assets/MarchingCubes.glb";
	HashMap<String, MeshConfig> mesh_library;
	CUI *ui_manager = nullptr;
	VBoxContainer *stats_vbox = nullptr;
	Panel *side_panel = nullptr;
	AcceptDialog *help_dialog = nullptr;
	std::vector<String> base_mesh_order;

	bool regenerate_mesh = false;

protected:
	static void _bind_methods();

public:
	MCNode();
	~MCNode();

	void _ready() override;

	void set_mesh_library_path(const String &p_path);
	String get_mesh_library_path() const;

	void set_regenerate_mesh(bool p_regenerate);
	bool get_regenerate_mesh() const;

	void load_mesh_library();
	void display_library();
	void generate_variants();

	// UI
	void setup_ui();
	void update_ui_counts();
	void _on_toggle_ui();
	void _on_gui_input(const Ref<InputEvent> &p_event);
	void _on_show_help();

	// Enum-driven generation
	void apply_transform_sequence(uint8_t p_base_hash, const Transform3D &p_base_t, const std::vector<MCTransform> &p_sequence, const String &p_name);
	void apply_4_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_6_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_8_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_12_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_12_mirror_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);
	void apply_24_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix);

	// Hash transformations
	static uint8_t rotate_y(uint8_t p_hash);
	static uint8_t rotate_x(uint8_t p_hash);
	static uint8_t rotate_z(uint8_t p_hash);
	static uint8_t scale_x(uint8_t p_hash);

	static String hash_to_binary(uint8_t p_hash);

	// Corner Helpers
	static inline bool is_c0(uint8_t h) { return h & (1 << 7); }
	static inline bool is_c1(uint8_t h) { return h & (1 << 6); }
	static inline bool is_c2(uint8_t h) { return h & (1 << 5); }
	static inline bool is_c3(uint8_t h) { return h & (1 << 4); }
	static inline bool is_c4(uint8_t h) { return h & (1 << 3); }
	static inline bool is_c5(uint8_t h) { return h & (1 << 2); }
	static inline bool is_c6(uint8_t h) { return h & (1 << 1); }
	static inline bool is_c7(uint8_t h) { return h & (1 << 0); }

	static inline void activate_c0(uint8_t &h) { h |= (1 << 7); }
	static inline void activate_c1(uint8_t &h) { h |= (1 << 6); }
	static inline void activate_c2(uint8_t &h) { h |= (1 << 5); }
	static inline void activate_c3(uint8_t &h) { h |= (1 << 4); }
	static inline void activate_c4(uint8_t &h) { h |= (1 << 3); }
	static inline void activate_c5(uint8_t &h) { h |= (1 << 2); }
	static inline void activate_c6(uint8_t &h) { h |= (1 << 1); }
	static inline void activate_c7(uint8_t &h) { h |= (1 << 0); }
};

} // namespace godot

#endif // MCNODE_H
