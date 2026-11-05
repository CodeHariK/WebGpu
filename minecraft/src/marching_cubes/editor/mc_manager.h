// MCManager: the in-game editor for an MCGrid (modes, mouse input, hover, side-panel UI).
//   mc_manager.cpp        lifecycle, node paths, save/load
//   mc_manager_input.cpp  mouse clicks: raycast once, then terrain edits or hand off to object mode
//   mc_object_mode.cpp    placing, selecting, dragging, deleting objects
//   mc_manager_inspect.cpp inspect mode: click a cell, see its tile + corners in a side view
//   mc_manager_hover.cpp  per-frame hover raycast and previews
//   mc_ui.cpp             side panel, stats, help
//   mc_manager_randomize.cpp   Randomize section of the side panel (grid corners by the L / M / H rules)
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
class Button;
class HBoxContainer;
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
	int randomize_seed = 0; // the last Randomize seed (0 = not randomized yet)
	Label *randomize_label = nullptr;
	Button *heightmap_button = nullptr; // "Heightmap mode: on / off"
	Button *tool_buttons[4] = {}; // the tool bar, by InteractionMode
	Button *undo_button = nullptr;
	Button *redo_button = nullptr;
	String pinned_name; // "00001100_la1" of the pinned library tile ("" = none)

	MCUI ui;
	GridStats grid_stats;
	PerfStats perf;
	MeshInstance3D *hover_box_node = nullptr; // For AxBxC preview
	Ref<StandardMaterial3D> hover_mat_yellow;
	Ref<StandardMaterial3D> wire_mat_ok; // placement wire box: fits
	Ref<StandardMaterial3D> wire_mat_blocked; // placement wire box: blocked
	Ref<StandardMaterial3D> hover_mat_red;
	Ref<StandardMaterial3D> hover_mat_cyan; // For multi-selection

	enum InteractionMode : uint8_t {
		MODE_TERRAIN,
		MODE_PLACE_OBJECT,
		MODE_DRAG_OBJECT,
		MODE_WATER, // heightmap mode: fill / drain water (mc_manager_tools.cpp)
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

	// Randomize section: the button and the last result's summary.
	void _setup_randomize_ui();
	// Randomize button: next seed, new grid corners by the L / M / H rules.
	void _on_randomize_grid();
	// Unpin button: Randomize no longer keeps a library tile.
	void _on_unpin();
	// Heightmap mode on / off (MCGrid::set_heightmap_mode); the button shows the state.
	void _on_toggle_heightmap();
	// --- tool bar + undo / redo (mc_manager_tools.cpp) ---
	// The tool buttons (one pressed at a time) and Undo / Redo, in the top bar.
	void _setup_tool_bar(HBoxContainer *p_bar);
	// Make tool p_mode (an InteractionMode) the active one.
	void _on_select_tool(int p_mode);
	// Press and mark ("● Name") the active tool's button.
	void _mark_active_tool();
	void _on_undo();
	void _on_redo();
	// Grey out Undo / Redo when there is nothing to undo / redo.
	void _update_history_buttons();
	// Cmd/Ctrl+Z undo, Cmd/Ctrl+Shift+Z or Cmd/Ctrl+Y redo; true when the key was used.
	bool _history_shortcut(const Ref<InputEvent> &p_event);
	// Water tool: left fills the clicked column's hole one tier deep, right drains it (one undo step).
	void _water_click(
			MCGrid *p_grid,
			const ClickHit &p_hit,
			MouseButton p_button
	);
	// Left click on a library tile (nearest hit): pin it, move the grid into a row behind the
	// library with the pinned column lined up with the tile, Randomize. False when no tile was hit.
	bool _try_pin_click(
			const Ref<InputEvent> &p_event,
			MCGrid *p_grid
	);

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
	// Debug views: terrain wireframe, water overlay, replay the last pour.
	void _on_toggle_wireframe();
	void _on_toggle_water_sampled(); // water mesh: sample grid or per cell
	void _on_water_drop(double p_value); // the water drop slider
	void _on_water_expand(double p_value); // the water expand slider
	void _on_water_spill(double p_value); // the spill reach slider
	void _on_water_spill_radius(double p_value); // the circle spill radius slider
	void _on_water_debug_stage(double p_value); // the water debug stage slider
	void _on_toggle_tile_seams(); // terrain: lines where the tiles meet
	void _on_toggle_water_outline_fill(); // flat water from the tiles or from the outlines
	void _on_water_fall_lip(double p_value); // the waterfall lip radius slider
	void _on_water_samples(double p_value); // the water samples per cell slider
	void _on_toggle_edge_corners(); // inactive corners with an edge to an active one
	void _on_toggle_water_debug();
	void _on_toggle_water_particles(); // shore foam + fall splashes on / off
	void _on_cycle_water_view(); // water mesh view: normal / wireframe / UV / UV + wireframe
	// recording an inspected spot (mc_manager_record.cpp): camera, cell, map, screenshot
	void _on_record_view(); // Debug tab: record the camera + the hovered cell
	void _record_inspect(
			MCGrid *p_grid,
			const Vector3i &p_cell,
			const String &p_how
	);
	Label *record_label = nullptr; // Debug tab: what was recorded last
	Label *water_stage_label = nullptr; // Water tab: what the debug stage shows
	Button *water_view_button = nullptr; // "Water view: <view>"
	void _on_replay_pour();
	// --- mc_manager_inspect.cpp: inspect mode (a clicked cell shown in a side view) ---
	struct InspectView {
		class PanelContainer *panel = nullptr; // bottom right; hidden until a cell is inspected
		Node3D *pivot = nullptr; // turns the camera round the cell
		class Camera3D *camera = nullptr;
		Node3D *preview = nullptr; // MCGrid::make_cell_preview of the cell
		class Label *info = nullptr; // MCGrid::get_cell_info
		float yaw = 0.6f;
		float pitch = -0.5f;
		float distance = 3.6f;
		bool spinning = true; // turns slowly until orbited by hand
	} inspect;
	void _on_toggle_inspect();
	// Inspect a cell from code (turns inspect mode on): marks it and shows it in the panel.
	void inspect_at_cell(const Vector3i &p_cell);
	bool _inspect_active(MCGrid *p_grid) const;
	void _setup_inspect_panel();
	void _place_inspect_camera();
	void _show_inspect_cell(
			MCGrid *p_grid,
			const Vector3i &p_cell
	);
	void _on_inspect_view_input(const Ref<InputEvent> &p_event);
	void _update_inspect_view(double p_delta);
	// Clicks in inspect mode (left: inspect the cell, right: clear); true when the event was used.
	bool _inspect_input(
			MCGrid *p_grid,
			const Ref<InputEvent> &p_event
	);
	// . , / step through the last pour's flood fill (water debug); true when the key was used.
	bool _pour_step_shortcut(const Ref<InputEvent> &p_event);
	// _unhandled_input (not _input): clicks on UI buttons are consumed by the GUI first and never
	// reach the terrain behind them.
	void _unhandled_input(const Ref<InputEvent> &p_event) override;
	// Input on the side panel (keeps clicks from reaching the terrain).
	void _on_gui_input(const Ref<InputEvent> &p_event);
	// Open the help dialog.
	void _on_show_help();
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
