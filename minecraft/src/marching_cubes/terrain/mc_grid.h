// MCGrid: the editable corner grid, split into chunks, and everything spawned for it. One class;
// its methods are grouped in folders by job (docs/MarchingCubes.md has every file):
//   mc_grid.cpp                      setup, properties, corner edits, corner/cell queries
//   1_shape/                         what each cell is
//     mc_grid_levels.cpp             each cell's level (L / M / H, core/mc_level_plan.h) and Randomize
//     mc_grid_heights.cpp            heightmap mode: one height per corner column, no overhangs, islands
//     mc_grid_groups.cpp             each cell's tile group (a, b …): one rolled per connected solid region
//   2_mesh/                          chunks into meshes
//     mc_chunk_builder.h/.cpp        tiles merged into one mesh (one surface per material)
//     mc_grid_cells.cpp              each chunk's tiles into its mesh + collider; dirty chunks rebuilt
//     mc_grid_collision.cpp          the chunks' colliders (shared tile shapes, or one trimesh per chunk)
//   edit/                            undo / redo, placed objects
//     mc_grid_undo.cpp               each edit records the old values it changes
//     mc_grid_objects.cpp            placed objects (with mc_placed_objects)
//   io/                              save / load
//     mc_grid_io.cpp                 version 1 files load; JSON for Blender
//     mc_grid_save_v2.cpp            format version 2: per-block layers (corners, or heights + water), zstd
//   debug/                           debug views
//     mc_grid_debug_corners.cpp      debug dots on the corners (+ their picking colliders)
//     mc_grid_debug_seams.cpp        lines where the tiles meet
//     mc_grid_debug_view.cpp         corner dot look, terrain wireframe
//     mc_grid_inspect.cpp            inspect mode: mark a cell, build its preview (tile + numbered corners) + info
//     mc_grid_hover.cpp              hover preview quads
//   ../water/                        the water layer (heightmap mode): ponds, rivers, sea, waterfalls, by
//                                    step; ../water/README.md has the order and what each folder holds
#ifndef MC_GRID_H
#define MC_GRID_H

#include "marching_cubes/core/mc_chunk.h"
#include "marching_cubes/core/mc_grid_dims.h"
#include "marching_cubes/core/mc_level_plan.h"
#include "marching_cubes/terrain/2_mesh/mc_chunk_builder.h"
#include "marching_cubes/terrain/edit/mc_placed_objects.h"
#include <cstdint>
#include <memory>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <unordered_set>
#include <utility>
#include <functional>
#include <unordered_map>
#include <vector>

namespace godot {

namespace mc_water {
struct Body; // mc_water_shape.h
}
// One fall placed in the world (mc_water_tiles.cpp), before it becomes a strip
// (mc_water_fall_strips.cpp): its top edge (ta, tb) on the surface, the same at the landing height
// (ba, bb), the way it faces, its height, the surface's flow at its ends.
struct PlacedFall {
	Vector3 ta;
	Vector3 tb;
	Vector3 ba;
	Vector3 bb;
	Vector3 out;
	float drop = 0.0f;
	Color flow_a; // the surface's flow COLOR at ta / tb
	Color flow_b;
	float depth_a = 0.0f; // the water's depth where the strip starts (the surface's own, baked)
	float depth_b = 0.0f;
	float stream = 0.0f; // 1: the fall of a stream
};
// One end of a fall's strip (_fall_ends, mc_water_fall_strips.cpp): the way it leaves the surface
// (smoothed along the falls) and its bevel radius (the smallest of every fall ending at that point),
// both the same for every strip ending there, so neighbouring strips share their end column.
struct FallEnd {
	Vector3 direction;
	float radius = 0.0f;
};
struct FallEnds {
	FallEnd end[2]; // at ta, at tb
};

// One edge segment of a body of water (water debug stage 3, mc_water_tiles.cpp), oriented with the
// water on its left seen from above, tagged by what lies beyond it.
struct WaterEdge {
	enum Kind : uint8_t {
		BANK = 0, // higher ground
		FALL = 1, // lower water / ground: a waterfall
	};
	Vector3 a;
	Vector3 b;
	Kind kind = BANK;
};

// The water mesh being built (mc_water_bodies.cpp): surface (with flow colour) and falls.
struct MCWaterArrays {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uvs; // world x z
	PackedVector2Array uv2s; // water.gdshader's form: x over the lip, y metres above the landing or -1 on a stream
	PackedColorArray colors; // flow: rg direction 0..1, b flows, a landing foam
	std::vector<Vector3> samples; // the wet sample points (debug overlay)
	PackedInt32Array indices; // welded surface triangles
	// the fall strips (in the same vertex arrays, mc_water_fall_strips.cpp): their triangles, drawn first
	PackedInt32Array first_indices;
	// the "pieces" water view (mc_water_view.cpp): where each mesh piece's triangles start in indices
	// (a body's flat tile, a fill region, a swept run) and in first_indices (a fall strip)
	std::vector<int64_t> pieces;
	std::vector<int64_t> first_pieces;
	void begin_piece() { pieces.push_back(indices.size()); }
	void begin_first_piece() { first_pieces.push_back(first_indices.size()); }
	std::vector<WaterEdge> edges; // water tiles: every edge segment of the water (debug stage 3 only)
	// per vertex: the water's depth in metres (surface to ground), baked (water.gdshader CUSTOM0.x)
	PackedFloat32Array depths;
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
	int head = 0; // this step's place in its pour's queue (0: the pour's first column)
	int pour = 0; // which pour it belongs to (trace_settle: one pour per source; fill_water: 0)
};

// One step of a water debug trace (step 2: the spills, held / stream, the flow), recorded as the
// builder's own functions run when asked (MCGrid::trace_water_step), shown with the same Back / Next
// buttons as the pour (mc_water_trace.cpp draws it).
struct MCTraceStep {
	enum Result : uint8_t {
		NONE,
		HELD, // the column's water is held (a full layer)
		STREAM, // the column's water is a stream (a thin sheet)
		SPILLED, // a column spill wets the column
		FLOWS, // the column's water flows (dir)
		STILL, // the column's water does not flow (not an outlet, not reached yet)
	};
	enum Mark : uint8_t {
		YES, // green: this neighbour lets it (holds it in, takes the spill, is an outlet ...)
		NO, // red: this neighbour stops it / makes it run out
		SAME, // grey: looked at, no effect
		LOOK, // cyan: the column this step comes from
	};
	int column = -1; // the column this step decides (-1: a step 3 - 4 trace step, see draw)
	Result result = NONE;
	Vector2 dir; // FLOWS: the direction (x, z)
	std::vector<std::pair<int, uint8_t>> marks; // neighbour column, Mark
	String text; // the explanation (the Debug tab shows it)
	// steps 3 - 4 (trace_water_cell): what this step draws, instead of the columns
	std::shared_ptr<struct MCDebugMesh> draw;
};

// One tile mesh, turned as a cell uses it, baked for the water (mc_water_banks_bake.cpp): its
// triangles (cell-local) and a top-down heightmap of them ((MCGrid::BAKE + 1)^2, NO_HIT: nothing).
struct MCBakedTile {
	std::vector<Vector3> triangles;
	std::vector<float> heights;
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
	int water_sample_rate = 8; // samples per cell side (water tiles)
	std::vector<Vector3> water_debug_samples; // the last build's wet sample points (debug overlay)
	// Water debug stages (mc_water_debug_stages.cpp): 0 the normal overlay; 1 .. 4 show how the
	// water's shape is built, one layer more each (see water_debug_stage_text).
	int water_debug_stage = 0;
	std::vector<uint8_t> water_spill_steps; // per column: the column spill's step (1: landing; 0: none)
	std::vector<WaterEdge> water_debug_edges; // stage 3: every cell's edge pieces, as baked
	class Node3D *water_debug_labels = nullptr; // stage 1: the spill step numbers
	bool debug_seams = false; // terrain: lines where the tiles meet (mc_grid_debug_seams.cpp)
	// Debug timing of chunk rebuilds (mc_grid_cells.cpp), summed until read: placing tiles, merging,
	// node + collider (microseconds), chunks rebuilt.
	uint64_t chunk_stage_usec[4] = { 0, 0, 0, 0 };
	uint64_t chunk_part_usec[4] = { 0, 0, 0, 0 }; // node + collider split: mesh node, collider shape(s), adding it, triangles
	int terrain_collision = 1; // TerrainCollision (mc_grid_collision.cpp), COLLISION_SHARED
	std::unordered_map<uint64_t, Ref<Shape3D>> shared_tile_shapes; // per tile mesh and turn (COLLISION_SHARED)
	mutable std::unordered_map<uint64_t, std::vector<float>> slice_fields; // per mesh + turn + height (kept)
	mutable int slice_winding = 0; // which way the tile triangles face (+1 / -1; 0: not known yet)
	mutable std::vector<uint64_t> slice_undecided; // slice fields baked before the winding was known
	// The tile meshes baked for the water (mc_water_banks_bake.cpp): per mesh and turn, kept for good.
	mutable std::unordered_map<uint64_t, MCBakedTile> baked_tiles;
	Ref<class ShaderMaterial> seams_material;
	int water_spill_reach = 1; // columns a diagonal spill wets (mc_water_2a_spill.cpp; 0: none)
	float water_fall_lip = 0.2f; // m: radius of a fall's rounded lip (mc_water_fall_strips.cpp)
	uint64_t water_rebuild_usec = 0; // debug: how long the last water rebuild took
	// debug: the last water rebuild by step (us): 2a spill, 2b held, 2c flow, 3 bodies, 4 tiles,
	// debug overlay, mesh upload (mutable: _emit_water_tiles is const and times its bodies)
	mutable uint64_t water_stage_usec[7] = {};
	mutable uint64_t water_bodies_nsec = 0; // _emit_water_tiles: the bodies, summed cell by cell (ns)
	float water_drop = 0.15f; // m held water's surface sits below the bank one tier up (UI slider)
	bool water_square = false; // blocky water with sharp corners (square tiles: Cliff_square.glb)
	int sea_level = 0; // randomize_heights fills the sea at this level (0: no sea)
	bool water_dirty = false; // the water mesh is rebuilt at the end of the frame
	MeshInstance3D *water_node = nullptr; // the water mesh: surface and waterfalls in one surface (mc_water_build.cpp)
	int water_view = 0; // WaterView: debug views of the water mesh (mc_water_view.cpp)
	Ref<Material> water_uv_material;
	Ref<Material> water_pieces_material; // the "pieces" view: vertex colour, cull disabled
	Vector2i water_piece_counts; // the last build's pieces: x flat tiles, y fall strips
	Ref<Material> water_wireframe_material;
	Ref<Material> water_material; // water.gdshader unless set: surfaces and falls, one material
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
	// The water debug trace the step buttons go through: 0 the pour (last_pour_trace: the last
	// fill_water, or trace_settle), 2 the spills, 3 held / stream, 4 the flow (water_trace).
	int water_trace_kind = 0;
	std::vector<MCTraceStep> water_trace;
	Vector2i water_trace_cell = Vector2i(-1, -1); // trace_water_cell's cell
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
	// --- water (../water/, README.md there) ---
	// Is column p_index wet (its water level above its ground)?
	bool _is_wet(int p_index) const;
	// Is column p_index the sea (wet, at sea_level; never when sea_level is 0)?
	bool _is_sea(size_t p_index) const;
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
	// r_trace (if given): every decision, with its reason (the step 2b debug trace).
	void _water_surfaces(
			std::vector<float> &r_surface,
			std::vector<uint8_t> &r_held,
			std::vector<MCTraceStep> *r_trace = nullptr
	) const;
	// The water mesh from baked water tiles, welded (mc_water_tiles.cpp).
	void _emit_water_tiles(
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
	// Which way each wet column flows: Color(dir x, dir z, 1 = flows, 1 = a fall lands here).
	// r_trace (if given): every decision, with its reason (the step 2c debug trace).
	std::vector<Color> _water_flow(
			const std::vector<float> &p_surface,
			std::vector<MCTraceStep> *r_trace = nullptr
	) const;
	String _column_name(int p_index) const; // "(x, z)" for debug text
	// The active water trace's step (step 2) in the overlay (mc_water_trace.cpp).
	void _add_water_trace_debug(
			struct MCDebugMesh &r_debug,
			const std::vector<float> &p_surface
	);
	// A breadth-first flood fill from p_column over the 4 neighbours: a neighbour is reached when
	// p_enter(from, to) (column indices) says so; columns in r_seen are skipped, reached ones are
	// added. Returns the columns in the order reached; r_trace (if given) records every step.
	std::vector<int> _flood(
			const Vector2i &p_column,
			const std::function<bool(size_t, size_t)> &p_enter,
			std::vector<uint8_t> &r_seen,
			std::vector<MCPourStep> *r_trace = nullptr
	) const;
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
	Ref<Material> _seams_overlay(); // the tile-seams overlay (its next pass: the wireframe when on)
	// Cell p_cell's tile mesh baked for the water (mc_water_banks_bake.cpp): its turned triangles and
	// top-down heightmap, once per mesh and turn (null: no tile).
	const MCBakedTile *_baked_tile(
			const Vector3i &p_cell,
			uint64_t *r_key = nullptr
	) const;
	// Water debug stages 1 .. 4 into the overlay (mc_water_debug_stages.cpp).
	void _add_water_stage_debug(
			struct MCDebugMesh &r_debug,
			const std::vector<float> &p_surface,
			const std::vector<uint8_t> &p_held,
			const std::vector<Color> &p_flow
	);
	// Build the water overlay from the surfaces, held flags and flow _rebuild_water worked out.
	void _rebuild_water_debug(
			const std::vector<float> &p_surface,
			const std::vector<uint8_t> &p_held,
			const std::vector<Color> &p_flow
	);
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
	// Corner columns along x and z, and the highest height a column can have.
	Vector2i get_column_counts() const { return _column_counts(); }
	int get_max_column_height() const { return _max_column_height(); }
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
	// Tile seams: a terrain overlay with a dark line wherever two tiles meet (the cell boundaries, every
	// 1 m in x, y and z) and a faint tint per tile (mc_grid_debug_seams.cpp).
	void set_debug_seams(bool p_on);
	bool is_debug_seams() const { return debug_seams; }
	// Water debug stage 0 .. 6 (turns the water debug overlay on when above 0).
	static constexpr int WATER_DEBUG_STAGES = 4; // the last stage (the mesh)
	void set_water_debug_stage(int p_stage);
	int get_water_debug_stage() const { return water_debug_stage; }
	static String water_debug_stage_text(int p_stage);
	// How the terrain collides (mc_grid_collision.cpp has both ways and their costs).
	enum TerrainCollision {
		COLLISION_MESH = 0, // one trimesh per chunk from every tile triangle
		COLLISION_SHARED = 1, // (default) one shape per tile mesh and turn, shared; chunks refer to them
		COLLISION_COUNT = 2,
	};
	void set_terrain_collision(int p_mode); // rebuilds every chunk
	int get_terrain_collision() const { return terrain_collision; }
	static const char *collision_name(int p_mode);
	Ref<Shape3D> _shared_tile_shape(
			const Ref<Mesh> &p_mesh,
			const Basis &p_turn
	);
	void _build_chunk_collider(
			class MeshInstance3D *p_node,
			const PackedVector3Array &p_faces,
			const std::vector<std::pair<Ref<Mesh>, Transform3D>> &p_tiles
	);
	// The chunk rebuild timings since the last call: [placing, merging, node + collider, chunks], reset.
	PackedInt64Array take_chunk_stage_usec() {
		PackedInt64Array a;
		for (uint64_t &u : chunk_stage_usec) {
			a.push_back(static_cast<int64_t>(u));
			u = 0;
		}
		for (uint64_t &u : chunk_part_usec) {
			a.push_back(static_cast<int64_t>(u));
			u = 0;
		}
		return a;
	}
	// The slice of cell p_cell's tile at cell-local height p_height as a signed distance field
	// ((BAKE + 1)^2, m, >= 0 in the rock), and its key (mc_water_banks_slice.cpp).
	const std::vector<float> &_slice_field(
			const Vector3i &p_cell,
			float p_height,
			uint64_t &r_key
	) const;
	// Debug (mc_water_banks_bake.cpp): cell p_cell's tile as the water's bake sees it: {hash, key (one per
	// mesh and turn), faces (its triangles, turned, cell-local)}; empty when the cell draws no tile.
	Dictionary get_cell_tile_debug(const Vector3i &p_cell) const;
	static const int BAKE = 16; // bake grid: squares per cell side
	static constexpr float NO_HIT = -1e6f; // no mesh there
	void set_water_sample_rate(int p_rate); // samples per cell side, 2 .. 32
	int get_water_sample_rate() const { return water_sample_rate; }
	// Diagonal spills (mc_water_2a_spill.cpp): how many columns a spill past a low corner wets
	// (0 .. 8; 0: none), and the water levels with them added (mesh build only, never stored).
	void set_water_spill_reach(int p_reach);
	int get_water_spill_reach() const { return water_spill_reach; }
	// The water levels with the column spills added; r_steps (if given): per column the step it was
	// spilled at (1: the landing past the low corner, 2, 3 ... as it spreads; 0: not spilled).
	std::vector<uint8_t> _spilled_levels(
			std::vector<uint8_t> *r_steps = nullptr,
			std::vector<MCTraceStep> *r_trace = nullptr
	) const;
	// The water mesh: one surface, one material, falls first (mc_water_build.cpp).
	Ref<ArrayMesh> _water_mesh(const MCWaterArrays &p_arrays) const;
	Ref<ArrayMesh> _water_pieces_mesh(const MCWaterArrays &p_arrays) const; // the "pieces" view (mc_water_view.cpp)
	// Falls as strips with a rounded lip and a flared foot (mc_water_fall_strips.cpp): the lip radius
	// (0 .. 0.4 m; the surface is trimmed back by it at falls), and building the strips.
	void set_water_fall_lip(float p_lip);
	float get_water_fall_lip() const { return water_fall_lip; }
	// Join falls' ends on a cell side that neighbouring cells put a little apart (mc_water_fall_strips.cpp).
	static void _join_fall_ends(std::vector<PlacedFall> &r_falls);
	// Smooth the joined fall lines across cells (mc_water_fall_strips.cpp).
	static void _relax_fall_chains(std::vector<PlacedFall> &r_falls);
	// Each fall's top points' directions (ta, tb), smoothed along the falls (mc_water_fall_strips.cpp).
	// Each fall's two ends: direction and bevel radius (mc_water_fall_strips.cpp).
	std::vector<FallEnds> _fall_ends(const std::vector<PlacedFall> &p_falls) const;
	// Cut the flat water away from under each strip's bevel, so it never pokes through
	// (mc_water_surface_cut.cpp). Call before _emit_fall_strips (it rewrites r_out.indices).
	void _cut_surface_under_falls(
			const std::vector<PlacedFall> &p_falls,
			const std::vector<FallEnds> &p_ends,
			MCWaterArrays &r_out
	) const;
	// The strips, from p_ends (_fall_ends) (mc_water_fall_strips.cpp).
	void _emit_fall_strips(
			const std::vector<PlacedFall> &p_falls,
			const std::vector<FallEnds> &p_ends,
			MCWaterArrays &r_out
	) const;
	// Metres along the joined falls at each fall's ends (ta, tb): the strips' UV.x.
	std::vector<std::pair<float, float>> _fall_chain(const std::vector<PlacedFall> &p_falls) const;
	// Is the water of cell (p_cx, p_cz) with its bed at p_bed drawn in the blocky style (the terrain
	// group of the bed's cell is one of water_blocky_groups)? (mc_water_bodies.cpp)
	bool is_water_blocky(
			int p_cx,
			int p_cz,
			int p_bed
	) const;
	int64_t get_water_rebuild_usec() const { return static_cast<int64_t>(water_rebuild_usec); }
	PackedInt64Array get_water_stage_usec() const {
		PackedInt64Array a;
		for (uint64_t u : water_stage_usec) {
			a.push_back(static_cast<int64_t>(u));
		}
		return a;
	}
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
	// Debug views of the water mesh (mc_water_view.cpp).
	enum WaterView {
		WATER_VIEW_NORMAL = 0,
		WATER_VIEW_WIREFRAME = 1, // the triangles over the water
		WATER_VIEW_UV = 2, // a UV checker on the surface and the falls
		WATER_VIEW_UV_WIREFRAME = 3,
		WATER_VIEW_PIECES = 4, // every mesh piece (a body's flat tile, a fall strip) its own colour
		WATER_VIEW_COUNT = 5,
	};
	void set_water_view(int p_view);
	int get_water_view() const { return water_view; }
	static const char *water_view_name(int p_view);
	Vector2i get_water_piece_counts() const { return water_piece_counts; } // x flat tiles, y fall strips
	void _apply_water_view();
	void set_debug_water(bool p_on);
	bool is_debug_water() const { return debug_water; }
	void replay_water_debug();
	// Step through the last pour's flood fill (turns the water overlay on): -1 shows the whole pour,
	// 0 .. count-1 one step. Keys in the editor: . next, , back (Shift: 10 at a time), / whole pour.
	void set_pour_step(int p_step);
	int get_pour_step() const { return pour_step; }
	int get_pour_step_count() const {
		return static_cast<int>(water_trace_kind == 0 ? last_pour_trace.size() : water_trace.size());
	}
	// Debug: record step 2 (2: the spills, 3: held / stream, 4: the flow) as the trace the step
	// buttons go through, without changing anything. Returns the number of steps.
	int trace_water_step(int p_kind);
	// Debug: steps 3 - 4 for one cell as the trace (kind 5; mc_water_trace_cell.cpp): its corners, each
	// body's corner classes and shape (3), its tile's samples, edge pieces and triangles, the tiles
	// placed round it (4). Returns the step count.
	int trace_water_cell(const Vector2i &p_cell);
	int get_water_trace_kind() const { return water_trace_kind; }
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
	// The columns settle_water pours from: every wet column but the sea, highest ground first.
	std::vector<int> _settle_sources() const;
	// The step-by-step trace emptied, for a new pour or settle to fill (mc_water_trace.cpp).
	void _reset_pour_trace();
	// Debug: record step 1 for the whole map as the pour trace (the step-by-step buttons), without
	// changing any water: every wet column is a source, highest ground first, and each pours into
	// what no earlier pour reached (settle_water's order). Returns the number of steps.
	int trace_settle();
	// Dry the connected water (any level), leaving the sea. Returns how many columns.
	int clear_water(const Vector2i &p_column);
	// randomize_heights fills the sea from the grid's corner at this level (0: no sea).
	void set_sea_level(int p_level);
	int get_sea_level() const { return sea_level; }
	void set_water_material(const Ref<Material> &p_material) { water_material = p_material; }
	Ref<Material> get_water_material() const { return water_material; }
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
