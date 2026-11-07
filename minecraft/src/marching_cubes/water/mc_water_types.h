// The water's data types that MCGrid's members and methods use (terrain/mc_grid.h): the falls placed
// in the world and their strips' ends, the mesh being built, the edge pieces, the debug trace steps
// and the tile meshes baked for the banks. (The water's shape types: 3_bodies/mc_water_shape.h.)
#ifndef MC_WATER_TYPES_H
#define MC_WATER_TYPES_H

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <cstdint>
#include <memory>
#include <utility>
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

} // namespace godot

#endif // MC_WATER_TYPES_H
