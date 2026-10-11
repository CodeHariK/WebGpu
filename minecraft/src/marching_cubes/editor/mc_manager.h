// MCManager: the in-game editor for an MCGrid (modes, mouse input, hover, side-panel UI).
//   mc_manager.cpp        lifecycle, node paths, save/load
//   mc_manager_input.cpp  mouse clicks: raycast once, then terrain edits or hand off to object mode
//   mc_object_mode.cpp    placing, selecting, dragging, deleting objects
//   mc_manager_hover.cpp  per-frame hover raycast and previews
//   mc_ui.cpp             side panel, stats, help
#ifndef MC_MANAGER_H
#define MC_MANAGER_H

#include <cstdint>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <vector>

namespace godot {

class MCNode;
class MCGrid;
class CUI;
class VBoxContainer;
class Panel;
class CUIModal;
class Label;
class InputEvent;
class MeshInstance3D;
class Camera3D;
class QuadMesh;

class MCManager : public Node3D {
	GDCLASS(MCManager,
			Node3D)

public:
	struct MCUI {
		CUI *manager = nullptr;
		VBoxContainer *stats_vbox = nullptr;
		VBoxContainer *variant_stats_vbox = nullptr;
		Panel *side_panel = nullptr;
		CUIModal *help_dialog = nullptr;
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

	// One mouse click resolved against the grid. Positions are in MCGrid space.
	struct ClickHit {
		Vector3 position; // hit point
		Vector3 normal; // hit surface normal
		Vector3i corner; // the corner whose debug cube was hit (terrain mode)
		MeshInstance3D *object = nullptr; // the placed object that was hit, if any
	};

	// --- mc_manager_input.cpp: turn a click into a ClickHit, dispatch by mode
	TypedArray<RID> _drag_group_exclusions() const; // dragged objects must not block the raycast
	bool _raycast_click(
			const Ref<InputEvent> &p_event,
			MCGrid *p_grid,
			ClickHit &r_hit
	);
	void _terrain_click(
			MCGrid *p_grid,
			const ClickHit &p_hit,
			MouseButton p_button
	);

	// --- mc_object_mode.cpp: placing, selecting, dragging, deleting objects
	void _place_click(
			MCGrid *p_grid,
			const ClickHit &p_hit
	);
	void _select_or_start_drag(
			MCGrid *p_grid,
			const ClickHit &p_hit,
			bool p_shift
	);
	void _start_drag(
			MCGrid *p_grid,
			MeshInstance3D *p_pivot
	);
	void _finish_drag(MCGrid *p_grid);
	void _delete_object(
			MCGrid *p_grid,
			MeshInstance3D *p_object
	);

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
	void _mc_capture(); // debug: save a screenshot + quit (--mcshot)
	void update_ui();
	void _process(double p_delta) override;
	void _on_toggle_ui();
	void _on_toggle_visual_corners();
	// _unhandled_input (not _input): clicks on UI buttons are consumed by the GUI first and never
	// reach the terrain behind them.
	void _unhandled_input(const Ref<InputEvent> &p_event) override;
	void _on_gui_input(const Ref<InputEvent> &p_event);
	void _on_show_help();
	void _on_toggle_placement_mode();
	void cancel_drag();
	void _clear_selection(); // un-highlight and forget selected objects
	void _on_save_terrain();
	void _on_load_terrain();
	void save_terrain(const String &p_path);
	void load_terrain(const String &p_path);
	void _initialize_previews();
	void _update_hover_box(
			const Vector3i &p_grid_pos,
			bool p_is_blocked
	);
	void _update_hover_raycast();
	uint8_t _get_cell_hash(const Vector3i &p_grid_pos);
};

} // namespace godot

#endif // MC_MANAGER_H
