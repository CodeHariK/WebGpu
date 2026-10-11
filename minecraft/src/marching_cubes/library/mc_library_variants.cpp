// MCNode: fill the 256 library entries from the modelled tiles by turning them about Y.
//
// Works for any tile set without a mode setting:
//   - a 68-tile set (one tile per Y-turn group) fills all 254 drawable hashes,
//   - a heightmap set (22 tiles) fills the 79 hashes without overhangs,
//   - a complete set has nothing left to fill.
// A modelled tile is never replaced. When two modelled tiles could fill the same gap, the one
// loaded first wins (base_mesh_order: fewest corners first, then lowest hash).
#include "marching_cubes/core/mc_hash.h"
#include "marching_cubes/core/mc_symmetry.h"
#include "marching_cubes/library/mc_library.h"
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

int MCNode::fill_y_rotations() {
	int generated = 0;
	for (uint8_t base_h : base_mesh_order) {
		for (int turns = 1; turns <= 3; turns++) {
			uint8_t h = base_h;
			Transform3D t = mesh_library[base_h].transform;
			mc::turn_y(h, t, turns);
			if (mesh_library[h].mesh.is_valid()) {
				continue; // modelled, or already filled by an earlier tile
			}
			MeshConfig config = mesh_library[base_h]; // mesh + all its variants
			config.transform = t;
			config.source_mesh = base_h;
			mesh_library[h] = config;
			generated++;
		}
	}
	return generated;
}

void MCNode::report_missing() const {
	// Empty (0) is never drawn; full (255) is optional: drawn only if the set has one.
	// A heightmap set leaves every overhang hash empty, so only the first few are listed.
	const int max_listed = 16;
	String missing;
	int missing_count = 0;
	for (int h = 1; h < 255; h++) {
		if (mesh_library[h].mesh.is_valid()) {
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
		UtilityFunctions::print("MCNode: All 254 tiles present.");
	} else {
		UtilityFunctions::print(
				"MCNode: ", missing_count, " tiles missing (cells with these are not drawn): ", missing
		);
	}
}

} // namespace godot
