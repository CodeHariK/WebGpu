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

class MCTileLibrary;
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

	struct GridStats {
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
	NodePath tile_library_path;

	MCUI ui;
	GridStats grid_stats;
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
	// Raycast the click into the grid; false when nothing (corner or object) was hit.
	bool _raycast_click(
			const Ref<InputEvent> &p_event,
			MCGrid *p_grid,
			ClickHit &r_hit
	);
	// Terrain mode: left click fills the corner next to the hit face, right click empties it.
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
	// Drag mode press: shift toggles selection, a plain click starts dragging the selection.
	void _select_or_start_drag(
			MCGrid *p_grid,
			const ClickHit &p_hit,
			bool p_shift
	);
	// Detach the selected objects from the grid and remember where they started.
	void _start_drag(
			MCGrid *p_grid,
			MeshInstance3D *p_pivot
	);
	// Drop the dragged objects where they are, or back where they started if blocked.
	void _finish_drag(MCGrid *p_grid);
	// Remove a placed object (and end the drag if it was being dragged).
	void _delete_object(
			MCGrid *p_grid,
			MeshInstance3D *p_object
	);

protected:
	// Inspector properties and the methods the UI buttons call.
	static void _bind_methods();

public:
	MCManager();
	~MCManager() override;

	// Outside the editor: defer initialize_all so the grid and library nodes are ready first.
	void _ready() override;

	// Path to the MCTileLibrary node.
	void set_tile_library_path(const NodePath &p_path);
	// Path to the MCTileLibrary node.
	NodePath get_tile_library_path() const;

	// Path to the MCGrid node this editor works on.
	void set_grid_path(const NodePath &p_path);
	// Path to the MCGrid node this editor works on.
	NodePath get_grid_path() const;

	// Load the tile library, build the grid, create the UI and previews, register with GameManager.
	void initialize_all();

	// UI & Performance
	void setup_ui();
	// Debug (--mcshot): save a screenshot to docs/png and quit.
	void _save_screenshot_and_quit(); // debug: save a screenshot + quit (--mcshot)
	// Refresh the stats labels (called 10 times a second).
	void update_ui();
	// Per frame: stats refresh and hover raycast.
	void _process(double p_delta) override;
	// UI button handlers.
	void _on_toggle_ui();
	// Show / hide the grid's debug corner cubes.
	void _on_toggle_visual_corners();
	// _unhandled_input (not _input): clicks on UI buttons are consumed by the GUI first and never
	// reach the terrain behind them.
	void _unhandled_input(const Ref<InputEvent> &p_event) override;
	// Input on the side panel (keeps clicks from reaching the terrain).
	void _on_gui_input(const Ref<InputEvent> &p_event);
	// Open the help dialog.
	void _on_show_help();
	// Cycle terrain -> place object -> drag object mode.
	void _on_toggle_placement_mode();
	// Put dragged objects back where they started.
	void cancel_drag();
	// Un-highlight and forget the selected objects.
	void _clear_selection(); // un-highlight and forget selected objects
	// Save / Load buttons (user://terrain.mct).
	void _on_save_terrain();
	// Load button (user://terrain.mct).
	void _on_load_terrain();
	// Copy for Blender button: the cells as JSON on the clipboard, for the add-on's playground.
	void _on_copy_for_blender(); // cells as JSON on the clipboard, for the Blender playground
	// Save / load the grid's corners (MCGrid::save_grid / load_grid).
	void save_terrain(const String &p_path);
	// Load corners from p_path (puts dragged objects back first).
	void load_terrain(const String &p_path);
	// Create the hover box and its materials.
	void _initialize_previews();
	// Show the placement box at a cell, red when blocked.
	void _update_hover_box(
			const Vector3i &p_grid_pos,
			bool p_is_blocked
	);
	// Raycast from the mouse every frame and update the hover previews.
	void _update_hover_raycast();
	// Hash of a cell in the grid (for the hash label).
	uint8_t _get_cell_hash(const Vector3i &p_grid_pos);
};

} // namespace godot

#endif // MC_MANAGER_H
