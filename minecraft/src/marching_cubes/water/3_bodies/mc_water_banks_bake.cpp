// MCGrid: each tile mesh baked once for the water (per mesh and turn, kept for good): its triangles,
// turned as the cell uses it (cell-local), and a top-down heightmap of them, (BAKE + 1)^2 points,
// each the highest point of that mesh straight above / below it (a vertical ray against its
// triangles; no physics), or NO_HIT. The per-mesh banks (mc_water_banks_slice.cpp) slice the
// triangles; the heightmap settles which way the triangles face.
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/water/3_bodies/mc_water_shape.h"
#include "marching_cubes/library/mc_library.h"
#include <godot_cpp/classes/mesh.hpp>
#include <algorithm>
#include <cmath>

namespace godot {

static_assert(MCGrid::BAKE == mc_water::BAKED_SIDE, "the baked grid and the water's must match");

namespace {

// The height of the turned triangles at (x, z) (cell-local), or NO_HIT.
float triangles_height(
		const std::vector<Vector3> &p_triangles,
		float p_x,
		float p_z
) {
	float best = MCGrid::NO_HIT;
	for (size_t t = 0; t + 2 < p_triangles.size(); t += 3) {
		const Vector3 &a = p_triangles[t];
		const Vector3 &b = p_triangles[t + 1];
		const Vector3 &c = p_triangles[t + 2];
		float area = (b.x - a.x) * (c.z - a.z) - (c.x - a.x) * (b.z - a.z);
		if (std::abs(area) < 1e-9f) {
			continue; // a wall: nothing from above
		}
		float u = ((b.x - p_x) * (c.z - p_z) - (c.x - p_x) * (b.z - p_z)) / area;
		float v = ((c.x - p_x) * (a.z - p_z) - (a.x - p_x) * (c.z - p_z)) / area;
		float w = 1.0f - u - v;
		const float slack = -1e-4f; // on an edge counts (no holes between neighbouring triangles)
		if (u >= slack && v >= slack && w >= slack) {
			best = std::max(best, a.y * u + b.y * v + c.y * w);
		}
	}
	return best;
}

} // namespace

// Cell p_cell's tile mesh baked (null: no tile); r_key (if given): its mesh + turn key.
const MCBakedTile *MCGrid::_baked_tile(
		const Vector3i &p_cell,
		uint64_t *r_key
) const {
	if (!tile_library) {
		return nullptr;
	}
	uint8_t hash = get_cell_hash(p_cell);
	if (hash == 0) {
		return nullptr;
	}
	const MCTile &tile = tile_library->get_group_tile(_group_at(p_cell), mc::LEVEL_LETTERS[_level_at(p_cell)], hash);
	if (tile.mesh.is_null()) {
		return nullptr;
	}
	Ref<Mesh> mesh = tile.mesh_for_cell(p_cell);
	if (mesh.is_null()) {
		return nullptr;
	}
	// one bake per mesh and turn (which way +x points, mirrored or not)
	Vector3 x_axis = tile.turn.basis.xform(Vector3(1, 0, 0));
	int quarter = static_cast<int>(std::lround(std::atan2(x_axis.z, x_axis.x) / 1.5707963f)) & 3;
	int mirrored = tile.turn.basis.determinant() < 0.0f ? 4 : 0;
	uint64_t key = (static_cast<uint64_t>(mesh->get_rid().get_id()) << 3) | static_cast<uint64_t>(quarter | mirrored);
	if (r_key) {
		*r_key = key;
	}
	auto found = baked_tiles.find(key);
	if (found != baked_tiles.end()) {
		return &found->second;
	}
	MCBakedTile baked;
	PackedVector3Array faces = mesh->get_faces();
	for (int64_t i = 0; i < faces.size(); i++) {
		baked.triangles.push_back(tile.turn.xform(faces[i]));
	}
	const int n = BAKE + 1;
	baked.heights.assign(static_cast<size_t>(n * n), NO_HIT);
	for (int j = 0; j < n; j++) {
		for (int i = 0; i < n; i++) {
			float x = -0.5f + static_cast<float>(i) / BAKE;
			float z = -0.5f + static_cast<float>(j) / BAKE;
			baked.heights[static_cast<size_t>(j * n + i)] = triangles_height(baked.triangles, x, z);
		}
	}
	return &baked_tiles.emplace(key, std::move(baked)).first->second;
}

Dictionary MCGrid::get_cell_tile_debug(const Vector3i &p_cell) const {
	Dictionary out;
	uint64_t key = 0;
	const MCBakedTile *tile = _baked_tile(p_cell, &key);
	if (!tile) {
		return out;
	}
	out["key"] = static_cast<int64_t>(key);
	PackedVector3Array faces;
	for (const Vector3 &v : tile->triangles) {
		faces.push_back(v);
	}
	out["hash"] = get_cell_hash(p_cell);
	out["faces"] = faces;
	return out;
}


} // namespace godot
