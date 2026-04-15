#ifndef MC_MANAGER_H
#define MC_MANAGER_H

#include <cstdint>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/node_path.hpp>

namespace godot {

class MCNode;
class MCGrid;
class CUI;
class VBoxContainer;
class Panel;
class AcceptDialog;
class Label;
class InputEvent;
class MeshInstance3D;
class Camera3D;
class QuadMesh;

class MCManager : public Node3D {
	GDCLASS(MCManager, Node3D)

public:
	struct MCUI {
		CUI *manager = nullptr;
		VBoxContainer *stats_vbox = nullptr;
		VBoxContainer *variant_stats_vbox = nullptr;
		Panel *side_panel = nullptr;
		AcceptDialog *help_dialog = nullptr;
		Label *hash_label = nullptr;
	};

	struct TerrainStats {
		NodePath path;
		Label *stats_label = nullptr;
	};

	struct PerfStats {
		Label *fps_label = nullptr;
		Label *draw_calls_label = nullptr;
		Label *meshes_label = nullptr;
		Label *collision_label = nullptr;
		Label *memory_label = nullptr;

		double update_timer = 0.0;
	};

private:
	NodePath mc_node_path;

	MCUI ui;
	TerrainStats terrain;
	PerfStats perf;
	MeshInstance3D *hover_box_node = nullptr; // For AxBxC preview
	Ref<StandardMaterial3D> hover_mat_yellow;
	Ref<StandardMaterial3D> hover_mat_red;
	Ref<StandardMaterial3D> hover_mat_cyan; // For multi-selection

	enum InteractionMode : uint8_t {
		MODE_TERRAIN,
		MODE_PLACE_OBJECT,
		MODE_DRAG_OBJECT,
	};

	struct SelectedObject {
		MeshInstance3D *node = nullptr;
		Vector3i original_grid_pos;
		Vector3i size;
		Vector3i relative_offset; // Offset from the "pivot" object being dragged
	};

	// Placement State
	InteractionMode interaction_mode = MODE_TERRAIN;
	Vector3i current_placement_size = Vector3i(1, 1, 1);

	// Multi-Selection State
	std::vector<MeshInstance3D *> selected_nodes;

	// Dragging State
	bool is_dragging = false;
	std::vector<SelectedObject> drag_group;
	bool drag_valid = false;

	// Selection Inspection
	Vector3i locked_grid_pos;

protected:
	static void _bind_methods();

public:
	MCManager();
	~MCManager() override;

	void _ready() override;

	void set_mc_node_path(const NodePath &p_path);
	NodePath get_mc_node_path() const;

	void set_terrain_path(const NodePath &p_path);
	NodePath get_terrain_path() const;

	void initialize_all();

	// UI & Performance
	void setup_ui();
	void update_ui();
	void _process(double p_delta) override;
	void _on_toggle_ui();
	void _on_toggle_collision_debug();
	void _on_toggle_visual_corners();
	void _input(const Ref<InputEvent> &p_event) override;
	void _on_gui_input(const Ref<InputEvent> &p_event);
	void _on_show_help();
	void _on_toggle_placement_mode();
	void cancel_drag();
	void _on_save_terrain();
	void _on_load_terrain();
	void _initialize_previews();
	void _update_hover_box(const Vector3i &p_grid_pos, bool p_is_blocked);
	void _update_hover_raycast();
	uint8_t _get_cell_hash(const Vector3i &p_grid_pos);
};

} // namespace godot

#endif // MC_MANAGER_H
