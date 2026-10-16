// MCTileLibrary: fill the 256 library entries from the modelled tiles by turning them about Y.
//
// Works for any tile set without a mode setting:
//   - a 68-tile set (one tile per Y-turn group) fills all 254 drawable hashes,
//   - a heightmap set (22 tiles) fills the 79 hashes without overhangs,
//   - a complete set has nothing left to fill.
// A modelled tile is never replaced. When two modelled tiles could fill the same gap, the one
// loaded first wins (loaded_hashes: fewest corners first, then lowest hash).
#include "marching_cubes/core/mc_hash.h"
#include "marching_cubes/core/mc_symmetry.h"
#include "marching_cubes/library/mc_library.h"
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// For each modelled tile of one level (load order) and each of its 3 turns: if that hash is still
// empty, use this tile turned. Never replaces a modelled tile. Returns how many were filled.
static int fill_level_by_y_turns(
		MCTile *tiles,
		const std::vector<uint8_t> &loaded_hashes
) {
	int generated = 0;
	for (uint8_t base_h : loaded_hashes) {
		for (int turns = 1; turns <= 3; turns++) {
			uint8_t h = base_h;
			Transform3D t = tiles[base_h].turn;
			mc::turn_y(h, t, turns);
			if (tiles[h].mesh.is_valid()) {
				continue; // modelled, or already filled by an earlier tile
			}
			MCTile config = tiles[base_h]; // mesh + all its variants
			config.turn = t;
			config.source_hash = base_h;
			tiles[h] = config;
			generated++;
		}
	}
	return generated;
}

// Fill M a, then the ground (L) and roof (H) levels. Returns how many M a entries were filled.
int MCTileLibrary::fill_by_y_turns() {
	int generated = fill_level_by_y_turns(tiles, loaded_hashes);
	fill_level_by_y_turns(ground.tiles, ground.loaded_hashes);
	fill_level_by_y_turns(roof.tiles, roof.loaded_hashes);
	return generated;
}

// Print the drawable hashes (1..254) that still have no tile (the first 16).
void MCTileLibrary::report_missing() const {
	// Empty (0) is never drawn; full (255) is optional: drawn only if the set has one.
	// A heightmap set leaves every overhang hash empty, so only the first few are listed.
	const int max_listed = 16;
	String missing;
	int missing_count = 0;
	for (int h = 1; h < 255; h++) {
		if (tiles[h].mesh.is_valid()) {
			continue;
		}
		if (missing_count < max_listed) {
			missing += (missing_count > 0 ? ", " : "") + mc::hash_to_binary((uint8_t)h);
		} else if (missing_count == max_listed) {
			missing += ", ...";
		}
		missing_count++;
	}
	if (missing_count == 0) {
		UtilityFunctions::print("MCTileLibrary: All 254 tiles present.");
	} else {
		UtilityFunctions::print(
				"MCTileLibrary: ", missing_count, " tiles missing (cells with these are not drawn): ", missing
		);
	}
}

} // namespace godot
