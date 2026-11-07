// Water step 4: one body's water tile (mc_water_tiles.cpp), visible to the step-by-step debug
// (water/debug/mc_water_trace_cell.cpp) so it can show the very tiles the builder uses.
//
// A tile is baked from a body's shape: sample the water field on a (rate + 1)^2 grid over the cell,
// trace the wet area's edge with marching squares (crossings bisected on the exact shape), then
// look just outside each edge piece: a bank -> a shore, anything lower -> a fall. Tiles are cached
// by tile_key: bodies whose corner classes (and style, rate, lip, spill, baked banks) match share one.
#ifndef MC_WATER_TILE_H
#define MC_WATER_TILE_H

#include "marching_cubes/water/3_bodies/mc_water_shape.h"
#include <cstdint>
#include <vector>

namespace godot {
namespace mc_water {

const float TILE_INSIDE = 1e-4f; // the shape value a sample needs to count as water

// What a body's corner is to it (step 3's corner classes).
enum CornerClass : uint32_t {
	BANK = 0, // ground at or above the water level: the water ends under it
	WET = 1, // one of the body's own corners
	LOWER = 2, // ground below the body's bed: the water falls down there
	DRY = 3, // dry ground of the bed's tier: a gap the water stops short of
};

// One baked tile: the surface triangles, the falls and the shores, in cell-local shape coordinates.
struct WaterTile {
	bool whole = false; // the body covers the cell: one quad
	std::vector<Vector2> surface; // triangles, 3 points each
	struct Fall {
		Vector2 a;
		Vector2 b;
		Vector2 out; // faces down the step (shape coordinates)
		bool to_dry = false; // lands on dry ground of the bed's tier (else: below the bed)
	};
	std::vector<Fall> falls;
	struct Shore { // where the surface meets a bank
		Vector2 a;
		Vector2 b;
		Vector2 out; // towards the bank
	};
	std::vector<Shore> shores;
	std::vector<Vector2> wet_samples; // the sample points inside the water
	std::vector<Vector2> rim; // the surface's edge inside the cell, as segments (point pairs)
};

CornerClass corner_class(
		const Body &p_body,
		int p_corner
);
// The cache key of a body's tile at sample rate p_rate (with its baked banks' key mixed in).
uint64_t tile_key(
		const Body &p_body,
		int p_rate
);
// Bake a body's tile (no cache): the surface triangulated by CDT over its edge, its side points
// and a coarse grid inside (the sample grid only finds the edge).
WaterTile bake_tile(
		const Body &p_body,
		int p_rate
);
// The cached tile of key p_key, or null when it hasn't been baked yet.
const WaterTile *cached_tile(uint64_t p_key);
// Forget every baked tile: when what they were baked from changes (the bank slices flipped, the
// sample rate, lip or style changed), so stale tiles aren't used and the cache doesn't keep growing.
void clear_tile_cache();

} // namespace mc_water
} // namespace godot

#endif // MC_WATER_TILE_H
