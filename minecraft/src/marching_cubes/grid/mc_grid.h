// MCGrid: the editable corner grid, split into chunks, and everything spawned for it.
//   mc_grid.cpp                setup, properties, corner edits, corner/cell queries
//   mc_grid_cells.cpp          the tiles of each chunk merged into one mesh + collider
//   mc_grid_levels.cpp         each cell's level (L / M / H, core/mc_level_plan.h) and Randomize
//   mc_grid_heights.cpp        heightmap mode: one height per corner column, no overhangs, islands
//   mc_grid_groups.cpp         each cell's tile group (a, b …): one rolled per connected solid region
//   mc_grid_water_sampled.cpp  the water mesh from a sample grid + marching squares (water_sampled)
//   mc_grid_water_mesh.cpp     the water mesh: surface + falls per cell with the cliff tiles' shape
//   mc_grid_water.cpp          the water layer (heightmap mode): ponds, rivers, sea, waterfalls
//   mc_grid_debug_corners.cpp  debug dots on the corners (+ their picking colliders)
//   mc_grid_inspect.cpp        inspect mode: mark a cell, build its preview (tile + numbered corners) + info
//   mc_grid_debug_view.cpp     debug views: corner dot look, terrain wireframe, water overlay + pour replay
//   mc_grid_hover.cpp          hover preview quads
//   mc_grid_objects.cpp        placed objects (with mc_spatial)
//   mc_grid_io.cpp             save / load (version 1 files load; JSON for Blender)
//   mc_grid_undo.cpp           undo / redo: each edit records the old values it changes
//   mc_grid_save_v2.cpp        save format version 2: per-block layers (corners, or heights + water), zstd
#ifndef MC_GRID_H
#define MC_GRID_H

#include "marching_cubes/core/mc_chunk.h"
#include "marching_cubes/core/mc_grid_dims.h"
#include "marching_cubes/core/mc_level_plan.h"
#include "marching_cubes/grid/mc_chunk_builder.h"
#include "marching_cubes/grid/mc_placed_objects.h"
#include <cstdint>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <unordered_set>
#include <utility>
#include <vector>

namespace godot {

namespace mc_water {
struct Body; // mc_water_shape.h
}

// The water mesh being built (mc_grid_water_mesh.cpp): surface (with flow colour) and falls.
struct MCWaterArrays {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uvs; // world x z
	PackedVector2Array uv2s; // x: 1 on a stream
	PackedColorArray colors; // flow: rg direction 0..1, b flows, a landing foam
	PackedVector3Array fall_vertices;
	PackedVector3Array fall_normals;
	PackedVector2Array fall_uvs; // x along the ground, y metres down
	PackedVector2Array fall_uv2s; // x metres above the landing
	// the falls as small pieces along the step edges (one per sub-grid triangle), before
	// _merge_falls joins them into one sheet per fall
	struct FallPiece {
		Vector3 a; // top edge ends (y unused)
		Vector3 b;
		float top = 0.0f;
		float bottom = 0.0f;
		Vector3 out; // faces down the step
	};
	std::vector<FallPiece> fall_pieces;
	std::vector<Vector3> samples; // water_sampled: the wet sample points (debug overlay)
};

// One step of a pour's flood fill (for the step-by-step debug): the column taken off the front of the
// queue and what happened to each of its 4 neighbours (+x, -x, +z, -z).
struct MCPourStep {
	enum Result : uint8_t {
		QUEUED, // ground not higher: water flows there, added to the back of the queue
		SEEN, // already in the queue or done: skipped
		HIGHER, // ground higher than here: a bank, rejected
		OUTSIDE, // off the grid
	};
	int column = -1; // corner column index
	Result result[4] = { OUTSIDE, OUTSIDE, OUTSIDE, OUTSIDE };
	int queue_size = 0; // queue length after this step (the queue holds every column found so far)
};

// One undoable edit: the old values it changed (index, value), and the decor seed and M roofs as
// they were when it began (mc_grid_undo.cpp).
struct MCEdit {
	std::vector<std::pair<int, uint8_t>> heights; // corner column index -> old height
	std::vector<std::pair<int, uint8_t>> water; // corner column index -> old water level
	std::vector<std::pair<int, uint8_t>> corners; // global corner key (x fastest) -> old solid bit
	int decor_seed = 0;
	std::vector<uint8_t> roof_m;
};

class Camera3D;
class BoxMesh;
class MCTileLibrary;
class FileAccess;
class Material;
class Mesh;
class MeshInstance3D;
class StandardMaterial3D;
class ShaderMaterial;
class MultiMeshInstance3D;

class MCGrid : public Node3D {
	GDCLASS(MCGrid,
			Node3D)

private:
	Vector3i grid_dims = Vector3i(1, 1, 1);
	Vector3i chunk_dims = Vector3i(16, 16, 16);
	MCTileLibrary *tile_library = nullptr;
	bool show_debug_corners = false;
	bool solid_floor = true; // floor corners (y = 0) locked solid; off: editable (random on Randomize)
	int decor_seed = 0; // socket decor roll; Randomize sets it to its seed (saved with the scene)
	std::vector<uint8_t> cell_levels; // per cell (mc::CellBlock order): which level's tile it draws
	std::vector<uint8_t> roof_m; // per cell: roof regions Randomize made M (kept through edits)
	std::vector<uint8_t> cell_groups; // per cell (mc::CellBlock order): its tile group letter
	mc::LevelPlan last_plan; // the last Randomize
	mc::LevelPin pin; // one library tile fixed in the grid (Randomize plans around it)
	bool heightmap_mode = false; // one height per corner column (no overhangs): mc_grid_heights.cpp
	std::vector<uint8_t> column_heights; // heightmap mode: per corner column (x + z * columns.x)
	bool lock_rim = true; // heightmap mode: outer columns locked to the floor (off: open edge)
	std::vector<uint8_t> water_levels; // per corner column, like column_heights (0 = dry)
	String water_blocky_groups = "b"; // groups whose water edges use the blocky shape
	bool water_sampled = true; // water mesh from a sample grid + marching squares (else per cell)
	int water_sample_rate = 8; // samples per cell side (water_sampled)
	std::vector<Vector3> water_debug_samples; // the last build's wet sample points (debug overlay)
	float water_drop = 0.15f; // m held water's surface sits below the bank one tier up (UI slider)
	bool water_square = false; // blocky water with sharp corners (square tiles: Cliff_square.glb)
	int sea_level = 0; // randomize_heights fills the sea at this level (0: no sea)
	bool water_dirty = false; // the water mesh is rebuilt at the end of the frame
	MeshInstance3D *water_node = nullptr; // surface + waterfalls (two surfaces)
	Ref<Material> water_material; // water_surface.gdshader unless set
	Ref<Material> waterfall_material; // waterfall.gdshader unless set
	std::vector<MCEdit> undo_stack; // oldest first
	std::vector<MCEdit> redo_stack;
	MCEdit recording; // the open edit (between begin_edit and end_edit)
	int edit_depth = 0; // > 0 while an edit is open
	std::unordered_set<int> recorded_heights; // what the open edit already recorded
	std::unordered_set<int> recorded_water;
	std::unordered_set<int> recorded_corners;
	Ref<Mesh> pin_mesh; // the pinned tile's own mesh, drawn at its cell
	std::vector<Chunk> chunks;
	MCPlacedObjects placed_list; // boxes placed in object mode
	int total_tiles_drawn = 0; // tiles drawn (cells with a mesh)
	int total_chunk_meshes = 0; // merged chunk meshes = draw objects for the tiles
	int total_debug_corners = 0;
	int total_cells = 0;
	Node3D *debug_corners_container = nullptr;

	Ref<Mesh> _debug_dot_solid; // corner dots (mc_grid_debug_view.cpp)
	Ref<Mesh> _debug_dot_empty;
	Ref<StandardMaterial3D> _debug_dot_empty_material;
	std::vector<Ref<StandardMaterial3D>> _debug_corner_materials; // solid dots, by corner y
	bool debug_wireframe = false; // chunk meshes get a wireframe overlay
	Ref<Material> _wireframe_material;
	bool debug_edge_corners = false; // inactive corners joined to an active one are shown
	Node3D *edge_corners_root = nullptr;
	MultiMeshInstance3D *edge_corners_dots = nullptr;
	MeshInstance3D *edge_corners_lines = nullptr;
	bool debug_water = false; // the water overlay is shown
	MeshInstance3D *water_debug_node = nullptr;
	Ref<ShaderMaterial> water_debug_material;
	std::vector<int> last_pour; // the columns the last fill_water reached, in the order it reached them
	std::vector<MCPourStep> last_pour_trace; // the last fill_water, step by step
	bool inspect_mode = false; // inspect mode: a clicked cell is shown in the editor's side view
	Vector3i inspect_cell_pos; // the inspected cell
	bool inspect_has_cell = false;
	Node3D *inspect_root = nullptr;
	MeshInstance3D *inspect_highlight = nullptr; // orange wire cube on the inspected cell
	Ref<Material> _inspect_cube_material;
	Ref<Material> _inspect_on_material;
	Ref<Material> _inspect_off_material;
	Ref<Mesh> _inspect_dot_mesh;
	int pour_step = -1; // step-by-step debug: the step shown (-1: the whole pour)
	class Label3D *pour_step_label = nullptr; // explains the shown step, over its column
	Node3D *hover_root = nullptr;
	MeshInstance3D *hover_cube = nullptr; // wire cube round the hovered corner
	MeshInstance3D *hover_face = nullptr; // the face the mouse is on: outline + faint fill


	// Index into chunks[] of the chunk at chunk coordinates (x, y, z).
	int _get_chunk_index(
			int x,
			int y,
			int z
	) const {
		return (y * grid_dims.x * grid_dims.z) + (z * grid_dims.x) + x;
	}

	// Mark the chunk owning this cell for a rebuild (after a corner edit).
	void _mark_cell_dirty(
			int gx,
			int gy,
			int gz
	);
	bool rebuild_queued = false; // a deferred rebuild of the dirty chunks is pending
	MCSurfaceCache surface_cache; // tile mesh arrays, read once (cleared when the library changes)
	// Ask for flush_dirty_chunks at the end of the frame (once, however many edits).
	void _queue_rebuild();
	// The grid's cells as a level block (cells per axis).
	mc::CellBlock _cell_block() const;
	// The level of a cell (mc::LEVEL_L / M / H); M outside the grid or before the first update.
	uint8_t _level_at(const Vector3i &p_cell) const;
	// Recompute every cell's level from the corners; cells whose level changed are marked dirty.
	void _update_levels();
	// The group of a cell ('a' outside the grid or before the first update).
	char _group_at(const Vector3i &p_cell) const;
	// Regions of the solid corners, a group rolled per region, each cell's group; cells whose group
	// changed are marked dirty (mc_grid_groups.cpp).
	void _update_groups(const std::vector<uint8_t> &p_corners);
	// The pin, while its cell still has the pinned hash (an edit there drops it); else nullptr.
	const mc::LevelPin *_live_pin() const;
	// Write one global corner into every chunk that stores it (no boundary check, no rebuild).
	void _write_corner(
			const Vector3i &p_grid_pos,
			bool p_active
	);
	// Merge every tile of the chunk into one mesh + one collider (replaces the old ones).
	void _build_chunk_mesh(Chunk &p_chunk);
	// Free the chunk's merged mesh node (and its collider) and reset its counters.
	void _free_chunk_mesh(Chunk &p_chunk);
	// Recreate or recolour the debug cube of one global corner after an edit.
	void _update_debug_corner(
			int gx,
			int gy,
			int gz
	);

	// Dimensions + all global <-> chunk-local conversions (bounds-checked): core/mc_grid_dims.h
	MCGridDims _dims() const { return MCGridDims{ grid_dims, chunk_dims }; }

	// Does any of the 6 neighbours of this global corner (inside the grid) have state p_state?
	// Looks across chunk borders, unlike Chunk::has_active_neighbor.
	bool _has_neighbor_with_state(
			const Vector3i &p_corner,
			bool p_state
	) const;

	// Placed objects live under their own container, so refresh_grid() never frees them.
	Node3D *placed_objects_container = nullptr;
	Ref<StandardMaterial3D> placed_object_material; // shared by every placed object
	// Free every placed object and empty the list (the grid was re-initialised).
	void _clear_placed_objects();

	// Free every chunk mesh and debug cube (placed objects and hover quads are kept).
	void _free_spawned_nodes();
	// Is this corner on the grid's outer shell? r_required_state is the state it must keep
	// (bottom layer solid, sides and top empty), so edits there are refused.
	bool _is_boundary_corner(
			int gx,
			int gy,
			int gz,
			bool &r_required_state
	) const;
	// Set the outer-shell corners of a chunk to their required state.
	void _initialize_boundaries(Chunk &p_chunk) const;
	// Create the debug cubes (with picking colliders) for a chunk's corners; returns how many.
	int _spawn_debug_corners(Chunk &p_chunk);

	// Create the three hover quads (one per axis) used to highlight the corner under the mouse.
	void _initialize_hover_previews();

	// --- heightmap mode (mc_grid_heights.cpp) ---
	// Corner columns per side (cells + 1), as (x, z).
	Vector2i _column_counts() const;
	// Index of corner column (x, z) in column_heights; -1 outside the grid.
	int _column_index(
			int p_x,
			int p_z
	) const;
	// The floor (1 with solid_floor, else 0) and the highest height (the top layer stays empty).
	int _min_column_height() const;
	int _max_column_height() const;
	// Outer columns (on the grid's edge) are locked to the floor.
	bool _is_edge_column(
			int p_x,
			int p_z
	) const;
	// Heights from the corners (the solid run up from the floor); overhangs above it are emptied.
	void _heights_from_corners();
	// Write one column's corners for a height (no checks, no rebuild).
	void _write_column(
			int p_x,
			int p_z,
			int p_height
	);
	// --- undo (mc_grid_undo.cpp) ---
	// Record the old value before it changes (only inside an edit, once per edit).
	void _record_height(int p_index);
	void _record_water(int p_index);
	void _record_corner(const Vector3i &p_corner);
	// Write an edit's old values back (recording the current ones into r_inverse), then rebuild.
	void _apply_edit(
			const MCEdit &p_edit,
			MCEdit &r_inverse
	);
	// --- save format version 2 (mc_grid_save_v2.cpp) ---
	// The corner columns chunk column (p_cx, p_cz) owns: [r_from, r_to) on x and z.
	void _block_columns(
			int p_cx,
			int p_cz,
			Vector2i &r_from,
			Vector2i &r_to
	) const;
	// Write the whole grid as version 2 (header, block sizes, blocks of layers).
	void _save_v2(const Ref<FileAccess> &p_file) const;
	// Read a version 2 file (magic + version already read) into temporaries, then apply it to the
	// grid (no rebuild); false (grid untouched) when anything is wrong.
	bool _load_v2(
			const Ref<FileAccess> &p_file,
			const String &p_path
	);
	// --- water (mc_grid_water.cpp) ---
	// Is column p_index wet (its water level above its ground)?
	bool _is_wet(int p_index) const;
	// Rebuild the water mesh at the end of the frame.
	void _queue_water();
	// The water mesh: a square per wet column, a waterfall sheet per step between wet neighbours.
	void _rebuild_water();
	// Is the waterfall from p_from (water at p_level) toward p_step cut into the land (banks at both
	// ends of the falling run)?
	bool _fall_framed(
			const Vector2i &p_from,
			const Vector2i &p_step,
			int p_level
	) const;
	// Each column's water surface (dry: -1e6) and whether it is held one layer deep (else a stream).
	void _water_surfaces(
			std::vector<float> &r_surface,
			std::vector<uint8_t> &r_held
	) const;
	// Join the fall pieces into whole falls (one quad per straight run, UV along the whole fall).
	void _merge_falls(MCWaterArrays &r_out) const;
	// The water mesh from a fine sample grid + marching squares (mc_grid_water_sampled.cpp).
	void _emit_water_sampled(
			const std::vector<float> &p_surface,
			const std::vector<uint8_t> &p_held,
			const std::vector<Color> &p_flow,
			MCWaterArrays &r_out
	) const;
	// The water bodies of one cell (one per level) and where each one's fall lands.
	void _water_bodies(
			int p_cx,
			int p_cz,
			const std::vector<float> &p_surface,
			const std::vector<uint8_t> &p_held,
			const std::vector<Color> &p_flow,
			std::vector<mc_water::Body> &r_bodies,
			std::vector<float> &r_bottoms
	) const;
	// Every cell's water (surface + falls) with the cliff tiles' shape, into r_out.
	void _emit_water_cells(
			const std::vector<float> &p_surface,
			const std::vector<uint8_t> &p_held,
			const std::vector<Color> &p_flow,
			MCWaterArrays &r_out
	) const;
	// Which way each wet column flows: Color(dir x, dir z, 1 = flows, 1 = a fall lands here).
	std::vector<Color> _water_flow(const std::vector<float> &p_surface) const;
	// Where water poured at p_column flows (ground not higher, 4 neighbours), skipping r_seen.
	std::vector<int> _flow_region(
			const Vector2i &p_column,
			std::vector<uint8_t> &r_seen,
			std::vector<MCPourStep> *r_trace = nullptr
	) const;
	// The step-by-step overlay of the last pour at pour_step (added to the water overlay).
	void _add_pour_step_debug(
			struct MCDebugMesh &r_debug,
			const std::vector<float> &p_surface
	);
	// --- debug views (mc_grid_debug_view.cpp) ---
	// A corner's debug node: solid -> a dot coloured by height, empty -> a small faint dot.
	void _style_debug_corner(
			MeshInstance3D *p_node,
			bool p_solid,
			int p_y
	);
	// The edge-corner view (rebuilt after edits while it is on); returns how many corners it shows.
	int _rebuild_edge_corners();
	// The wireframe overlay on a chunk mesh (on or off by debug_wireframe).
	void _apply_wireframe(MeshInstance3D *p_node);
	// Build the water overlay from the surfaces, held flags and flow _rebuild_water worked out.
	void _rebuild_water_debug(
			const std::vector<float> &p_surface,
			const std::vector<uint8_t> &p_held,
			const std::vector<Color> &p_flow
	);
	// The sea from p_column: every connected column whose ground is below p_level.
	std::vector<int> _sea_region(
			const Vector2i &p_column,
			int p_level
	) const;
	void _set_water(
			const std::vector<int> &p_region,
			int p_level
	);
	void _flood_sea(
			const Vector2i &p_column,
			int p_level
	);
	// The M roofs for the current corners, MCHeightmap's rule, seeded.
	void _roll_roof_m(uint32_t p_seed);
	// set_corner in heightmap mode: fill -> column rises to just above it, empty -> drops below it.
	void _set_corner_as_height(
			const Vector3i &p_grid_pos,
			bool p_active
	);

protected:
	// Inspector properties and the methods scripts / MCManager call.
	static void _bind_methods();

public:
	MCGrid();
	~MCGrid() override;

	// Allocate a grid of p_chunks_* chunks of p_chunk_dims_* cells, all corners empty except the
	// solid floor; drops placed objects. p_refresh builds the meshes right away.
	void initialize_grid(
			int p_chunks_x,
			int p_chunks_y,
			int p_chunks_z,
			int p_chunk_dims_x,
			int p_chunk_dims_y,
			int p_chunk_dims_z,
			bool p_refresh = true
	);
	// Rebuild every chunk mesh and debug cube from the corner data.
	void refresh_grid();
	// Rebuild the chunks changed by edits now. Edits queue this for the end of the frame, so many
	// edits in one frame (a brush, a generator) rebuild each chunk once.
	void flush_dirty_chunks();
	// Turn one global corner on or off (outer-shell corners are refused) and queue a rebuild of the
	// chunks whose cells changed.
	void set_corner(
			const Vector3i &p_grid_pos,
			bool p_active
	);
	// Is this global corner solid? Outside the grid counts as empty.
	bool is_corner_active(const Vector3i &p_grid_pos) const;
	// Would a p_size box with its lower corner at cell p_cell overlap solid terrain?
	bool is_area_blocked_by_grid(
			const Vector3i &p_cell,
			const Vector3i &p_size
	) const;
	// Does p_aabb (grid space) overlap any placed object?
	bool is_area_blocked_by_objects(const AABB &p_aabb) const;
	// Place a p_size box at cell p_cell (no checks: call the is_area_blocked_* functions first).
	void add_placed_object(
			const Vector3i &p_cell,
			const Vector3i &p_size
	);
	// Remove a placed object and free its node.
	void remove_placed_object(Node *p_node);
	// Take a placed object out of the list but keep its node (drag start).
	void detach_placed_object(Node *p_node);
	// The placed object whose node is p_node, or nullptr.
	const PlacedObject *get_placed_object(Node *p_node) const;

	// The 8-bit corner hash of the cell at p_pos (0 outside the grid).
	uint8_t get_cell_hash(const Vector3i &p_pos) const;
	// The tile group of the cell at p_pos: "a", "b" … ("" outside the grid).
	String get_cell_group(const Vector3i &p_pos) const;
	// The level of the cell at p_pos: 'l', 'm' or 'h' (as a String for scripts; "" outside).
	String get_cell_level(const Vector3i &p_pos) const;
	// New random corners following the level rules (L bottom layer, H on column tops and beside H,
	// L -> M -> H up a column), the best of p_tries layouts for p_seed; rebuilds the grid. Returns
	// a summary: "seed 3: 120 cells, L 40 M 50 H 30, 0 missing".
	String randomize_levels(
			int p_seed,
			int p_tries = 300
	);
	// Pin one tile (hash, level 'l' / 'm' / 'h', its mesh): its cell is the middle column, in the
	// bottom layer for L, one up for M, the top layer for H. Randomize keeps it and plans around it.
	void set_pin(
			int p_hash,
			const String &p_level,
			const Ref<Mesh> &p_mesh
	);
	// Remove the pin (the cell keeps its corners until the next Randomize).
	void clear_pin();
	// The pinned cell, or (-1, -1, -1) when nothing is pinned.
	Vector3i get_pin_cell() const;
	// The last Randomize as data: hashes / levels (PackedByteArray, x fastest, then y, then z;
	// 0 L, 1 M, 2 H), size, gaps, column_breaks, bottom_solved.
	Dictionary get_last_plan() const;
	// Heightmap mode: every corner column has one height, so no overhangs; edits (set_corner,
	// clicks) move whole columns. Turning it on flattens a live grid's overhangs.
	void set_heightmap_mode(bool p_on);
	bool get_heightmap_mode() const { return heightmap_mode; }
	// Height of corner column (x, z) = its solid corners (y < height); -1 outside / not in the mode.
	int get_column_height(const Vector2i &p_column) const;
	// Set a column's height (clamped to floor .. top - 1; outer columns locked); queues a rebuild.
	void set_column_height(
			const Vector2i &p_column,
			int p_height
	);
	// An island from noise for p_seed (turns heightmap mode on): tiers rising to the middle, the rim
	// at the floor, M roofs rolled; rebuilds the grid. Returns "island 3: heights 0x40 1x30 …".
	String randomize_heights(int p_seed);
	// Heightmap mode: outer columns locked to the floor (default), or editable with the terrain
	// ending open at the grid's edge (like MCHeightmap).
	void set_lock_rim(bool p_lock);
	bool get_lock_rim() const { return lock_rim; }
	// MCHeightmap's heightmap_data (p_size columns, a column solid up to and including its value)
	// into the grid (corner columns = p_size, or p_size + 2 with the rim locked), with MCHeightmap's
	// M-roof roll; turns heightmap mode on, rebuilds. Returns "" or what was wrong / clipped.
	String load_heights(
			const PackedByteArray &p_data,
			const Vector2i &p_size
	);
	// Undo / redo: everything between begin_edit and end_edit is one step (the editor wraps each
	// click, Randomize, water fill). Cleared on initialize / load / heightmap mode switch.
	void begin_edit();
	void end_edit();
	bool undo();
	bool redo();
	bool can_undo() const;
	bool can_redo() const;
	void clear_history();
	// Water (heightmap mode): level of a corner column (0 = dry; wet when above its ground).
	int get_water_level(const Vector2i &p_column) const;
	void set_water_level(
			const Vector2i &p_column,
			int p_level
	);
	// Waterscaping: the column becomes water (ground dug one tier, water at its old height). False if
	// it is wet already, at the floor or locked.
	bool paint_water(const Vector2i &p_column);
	// Pour water at a column (p_level: its ground + 1 normally, at most one layer deep): a flood fill
	// onto 4-neighbours whose ground is not higher (spreads, or falls and carries on), every reached
	// column one layer deep. Returns how many columns.
	int fill_water(
			const Vector2i &p_column,
			int p_level
	);
	// Debug views: terrain wireframe; water overlay (held / stream / sea, flow arrows, framed / open
	// falls, the last pour's order); replay the last pour.
	void set_debug_wireframe(bool p_on);
	bool is_debug_wireframe() const { return debug_wireframe; }
	// Which water mesh builder: sampled (sample grid + marching squares) or per cell.
	void set_water_sampled(bool p_on);
	bool get_water_sampled() const { return water_sampled; }
	void set_water_sample_rate(int p_rate); // samples per cell side, 2 .. 32
	int get_water_sample_rate() const { return water_sample_rate; }
	// How far held water's surface sits below the bank one tier up (0 .. 0.95 m, default 0.15).
	void set_water_drop(float p_drop);
	float get_water_drop() const { return water_drop; }
	// The surface height of held water at level p_level (grid space).
	float _surface_y(int p_level) const;
	// Blocky water with sharp corners, for the square tile set (no rounding).
	void set_water_square(bool p_on);
	bool get_water_square() const { return water_square; }
	// Groups (letters) whose water edges use the blocky shape, like their tiles (default "b").
	void set_water_blocky_groups(const String &p_groups);
	String get_water_blocky_groups() const { return water_blocky_groups; }
	// Edge corners: every inactive corner with an edge to an active corner (a dot + those edges).
	void set_debug_edge_corners(bool p_on);
	bool is_debug_edge_corners() const { return debug_edge_corners; }
	void set_debug_water(bool p_on);
	bool is_debug_water() const { return debug_water; }
	void replay_water_debug();
	// Step through the last pour's flood fill (turns the water overlay on): -1 shows the whole pour,
	// 0 .. count-1 one step. Keys in the editor: . next, , back (Shift: 10 at a time), / whole pour.
	void set_pour_step(int p_step);
	int get_pour_step() const { return pour_step; }
	int get_pour_step_count() const { return static_cast<int>(last_pour_trace.size()); }
	// What the shown step did, in words ("" when showing the whole pour).
	String get_pour_step_text() const;
	// Inspect mode (mc_grid_inspect.cpp): mark a cell (orange wire cube) for the editor's side view;
	// the terrain is never changed. inspect_cell: false if the cell is empty (still marked).
	void set_inspect_mode(bool p_on);
	bool is_inspect_mode() const { return inspect_mode; }
	bool inspect_cell(const Vector3i &p_cell);
	void clear_inspect();
	Vector3i get_inspect_cell() const; // (-1, -1, -1) when none
	// What the side view shows: the cell's tile (+ wireframe), its cube and 8 numbered corner dots,
	// centred on the origin (a new node; the caller owns it). And the text beside it.
	Node3D *make_cell_preview(const Vector3i &p_cell);
	String get_cell_info(const Vector3i &p_cell) const;
	// Let all standing water flow again (after a load or a terrain edit). Returns columns changed.
	int settle_water();
	// Dry the connected water (any level), leaving the sea. Returns how many columns.
	int clear_water(const Vector2i &p_column);
	// randomize_heights fills the sea from the grid's corner at this level (0: no sea).
	void set_sea_level(int p_level);
	int get_sea_level() const { return sea_level; }
	void set_water_material(const Ref<Material> &p_material) { water_material = p_material; }
	Ref<Material> get_water_material() const { return water_material; }
	void set_waterfall_material(const Ref<Material> &p_material) { waterfall_material = p_material; }
	Ref<Material> get_waterfall_material() const { return waterfall_material; }
	// Write the grid to a .mct file, version 2 (mc_grid_save_v2.cpp): heightmap mode saves heights +
	// water per chunk column, corner mode the corner bits per chunk; zstd per layer.
	void save_grid(const String &p_path);
	// Read a .mct file (version 1 or 2); the grid only changes once the whole file has been validated.
	void load_grid(const String &p_path);
	// Every non-empty cell as JSON text, for the Blender add-on's playground ("Paste from Godot").
	String cells_to_json() const;

	// Show the hover quads on the face of the corner under the mouse (p_corner_pos, grid space).
	void update_hover_preview(
			const Vector3 &p_corner_pos,
			const Vector3 &p_hit_normal,
			Camera3D *p_camera
	);
	// Hide the hover quads.
	void hide_hover_preview();
	// Nothing is built here: MCManager calls initialize_grid once the tile library is loaded.
	void _ready() override;

	// Chunks per axis (Inspector; takes effect at the next initialize_grid).
	void set_grid_dims(const Vector3i &p_size);
	// Chunks per axis.
	Vector3i get_grid_dims() const { return grid_dims; }

	// Cells per chunk per axis (Inspector; takes effect at the next initialize_grid).
	void set_chunk_dims(const Vector3i &p_size);
	// Cells per chunk per axis.
	Vector3i get_chunk_dims() const { return chunk_dims; }

	// Floor corners locked solid (default) or editable / random (needed for L ground tiles).
	void set_solid_floor(bool p_solid);
	bool get_solid_floor() const { return solid_floor; }
	void set_decor_seed(int p_seed);
	int get_decor_seed() const { return decor_seed; }

	// Show or hide the debug corner cubes.
	void set_debug_corners_visible(bool p_visible);
	// Are the debug corner cubes shown?
	bool is_debug_corners_visible() const;
	// Turn the debug cubes' picking colliders on or off.
	void set_corner_collision_enabled(bool p_enabled);

	// Stats for the editor panel.
	int get_total_tiles_drawn() const { return total_tiles_drawn; }
	// Merged chunk meshes (one draw object each).
	int get_total_chunk_meshes() const { return total_chunk_meshes; }
	// Debug corner cubes spawned.
	int get_total_debug_corners() const { return total_debug_corners; }
	// Cells in the whole grid.
	int get_total_cells() const { return total_cells; }

	// The tile library that supplies a mesh for every cell hash.
	void set_tile_library(MCTileLibrary *p_node);
	// The tile library in use.
	MCTileLibrary *get_tile_library() const;

	// Inspector property behind set_debug_corners_visible.
	void set_show_debug_corners(bool p_show);
	// Inspector property: debug corner cubes shown?
	bool get_show_debug_corners() const;
};

} // namespace godot

#endif // MC_GRID_H
