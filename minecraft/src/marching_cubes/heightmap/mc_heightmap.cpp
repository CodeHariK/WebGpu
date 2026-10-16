/**
 * @file mc_heightmap.cpp
 * @brief Node for generating 2.5D Marching Squares terrain using Marching Cubes meshes.
 * Module Path: src/marching_cubes/mc_heightmap.cpp
 * Build Dependencies: godot-cpp, mc_heightmap.h, mc.h
 */

#include "marching_cubes/heightmap/mc_heightmap.h"
#include "marching_cubes/library/mc_library.h"
#include <algorithm>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Inspector properties and the generate / clear methods.
void MCHeightmap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize_random_noise"), &MCHeightmap::initialize_random_noise);
	ClassDB::bind_method(
			D_METHOD("initialize_random_noise_and_generate"), &MCHeightmap::initialize_random_noise_and_generate
	);

	ClassDB::bind_method(D_METHOD("set_heightmap_data", "data"), &MCHeightmap::set_heightmap_data);
	ClassDB::bind_method(D_METHOD("get_heightmap_data"), &MCHeightmap::get_heightmap_data);
	ADD_PROPERTY(
			PropertyInfo(Variant::PACKED_BYTE_ARRAY, "heightmap_data"), "set_heightmap_data", "get_heightmap_data"
	);

	ClassDB::bind_method(D_METHOD("set_tile_library", "node"), &MCHeightmap::set_tile_library);
	ClassDB::bind_method(D_METHOD("get_tile_library"), &MCHeightmap::get_tile_library);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "tile_library", PROPERTY_HINT_NODE_TYPE, "MCTileLibrary"), "set_tile_library",
			"get_tile_library"
	);

	ClassDB::bind_method(D_METHOD("set_cell_spacing", "spacing"), &MCHeightmap::set_cell_spacing);
	ClassDB::bind_method(D_METHOD("get_cell_spacing"), &MCHeightmap::get_cell_spacing);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_spacing"), "set_cell_spacing", "get_cell_spacing");

	ClassDB::bind_method(D_METHOD("set_height_scale", "scale"), &MCHeightmap::set_height_scale);
	ClassDB::bind_method(D_METHOD("get_height_scale"), &MCHeightmap::get_height_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_scale"), "set_height_scale", "get_height_scale");

	ClassDB::bind_method(D_METHOD("set_grid_dims", "size"), &MCHeightmap::set_grid_dims);
	ClassDB::bind_method(D_METHOD("get_grid_dims"), &MCHeightmap::get_grid_dims);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "grid_dims"), "set_grid_dims", "get_grid_dims");

	ClassDB::bind_method(D_METHOD("set_show_debug_spheres", "show"), &MCHeightmap::set_show_debug_spheres);
	ClassDB::bind_method(D_METHOD("get_show_debug_spheres"), &MCHeightmap::get_show_debug_spheres);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_debug_spheres"), "set_show_debug_spheres", "get_show_debug_spheres");

	ClassDB::bind_method(D_METHOD("set_noise_source", "noise"), &MCHeightmap::set_noise_source);
	ClassDB::bind_method(D_METHOD("get_noise_source"), &MCHeightmap::get_noise_source);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "noise_source", PROPERTY_HINT_RESOURCE_TYPE, "Noise"), "set_noise_source",
			"get_noise_source"
	);

	ClassDB::bind_method(D_METHOD("generate_terrain"), &MCHeightmap::generate_terrain);
	ClassDB::bind_method(D_METHOD("clear_terrain"), &MCHeightmap::clear_terrain);
}

MCHeightmap::MCHeightmap() = default;

// No clear_terrain() here: the spawned meshes are children and are freed with the node, and
// touching the scene tree from the C++ destructor is unsafe during engine teardown.
MCHeightmap::~MCHeightmap() = default;

// Set the noise and regenerate when it changes (outside the editor).
void MCHeightmap::set_noise_source(const Ref<Noise> &p_noise) {
	if (noise_source == p_noise) {
		return;
	}
	if (noise_source.is_valid()) {
		noise_source->disconnect("changed", Callable(this, "initialize_random_noise_and_generate"));
	}
	noise_source = p_noise;
	if (noise_source.is_valid()) {
		noise_source->connect("changed", Callable(this, "initialize_random_noise_and_generate"));
	}
	initialize_random_noise_and_generate();
}

// New random heights, then rebuild the terrain.
void MCHeightmap::initialize_random_noise_and_generate() {
	initialize_random_noise();
	generate_terrain();
}

// Clear and spawn one tile per non-empty cell from the heightmap (and the debug spheres).
void MCHeightmap::generate_terrain() {
	clear_terrain();
	int required_size = grid_dims.x * grid_dims.y;
	if (heightmap_data.size() < required_size) {
		UtilityFunctions::print("MCHeightmap: Heightmap data size must be at least ", required_size, " bytes.");
		return;
	}
	if (!tile_library) {
		UtilityFunctions::print("MCHeightmap: MCTileLibrary reference is not set.");
		return;
	}

	const uint8_t *data = heightmap_data.ptr();
	int top = 0;
	for (int i = 0; i < required_size; i++) {
		top = std::max(top, static_cast<int>(data[i]));
	}
	// Cells: one fewer than the height columns per side, up to one above the highest column.
	mc::CellBlock block{ grid_dims.x - 1, std::min(top + 1, 254) + 1, grid_dims.y - 1 };
	std::vector<uint8_t> levels = _cell_levels(block, data);
	for (int z = 0; z < grid_dims.y - 1; z++) {
		for (int x = 0; x < grid_dims.x - 1; x++) {
			uint8_t h00 = data[x + z * grid_dims.x];
			uint8_t h10 = data[(x + 1) + z * grid_dims.x];
			uint8_t h11 = data[(x + 1) + (z + 1) * grid_dims.x];
			uint8_t h01 = data[x + (z + 1) * grid_dims.x];

			uint8_t h_min = h00;
			if (h10 < h_min)
				h_min = h10;
			if (h11 < h_min)
				h_min = h11;
			if (h01 < h_min)
				h_min = h01;

			uint8_t h_max = h00;
			if (h10 > h_max)
				h_max = h10;
			if (h11 > h_max)
				h_max = h11;
			if (h01 > h_max)
				h_max = h01;

			int start_y = (h_min > 0) ? (h_min - 1) : 0;
			int end_y = (h_max < 255) ? h_max : 254;

			for (int y = start_y; y <= end_y; y++) {
				uint8_t hash = _get_cell_hash(x, y, z, data);
				if (hash > 0 && hash < 255) {
					uint8_t level = y < block.y ? levels[static_cast<size_t>(block.cell_index(x, y, z))] : mc::LEVEL_M;
					_spawn_cell_mesh(x, y, z, hash, level);
				}
			}
		}
	}

	if (show_debug_spheres) {
		_spawn_debug_spheres(data);
	}
}

// Remove the spawned tiles.
void MCHeightmap::clear_terrain() { _clear_terrain_meshes(); }

// Free every spawned tile and debug sphere.
void MCHeightmap::_clear_terrain_meshes() {
	TypedArray<Node> children = get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (child) {
			remove_child(child);
			child->queue_free();
		}
	}
	debug_spheres_container = nullptr;
}

// A corner is solid when it is below its column's height.
bool MCHeightmap::_is_corner_active(
		int x,
		int y,
		int z,
		const uint8_t *data_ptr
) const {
	uint8_t height = data_ptr[x + z * grid_dims.x];
	return y <= height;
}

// 8-bit hash of a cell from its 8 corners (same bit order as MCGrid).
uint8_t MCHeightmap::_get_cell_hash(
		int x,
		int y,
		int z,
		const uint8_t *data_ptr
) const {
	uint8_t hash = 0;
	if (_is_corner_active(x, y, z + 1, data_ptr))
		hash |= (1 << 0);
	if (_is_corner_active(x + 1, y, z + 1, data_ptr))
		hash |= (1 << 1);
	if (_is_corner_active(x + 1, y, z, data_ptr))
		hash |= (1 << 2);
	if (_is_corner_active(x, y, z, data_ptr))
		hash |= (1 << 3);
	if (_is_corner_active(x, y + 1, z + 1, data_ptr))
		hash |= (1 << 4);
	if (_is_corner_active(x + 1, y + 1, z + 1, data_ptr))
		hash |= (1 << 5);
	if (_is_corner_active(x + 1, y + 1, z, data_ptr))
		hash |= (1 << 6);
	if (_is_corner_active(x, y + 1, z, data_ptr))
		hash |= (1 << 7);
	return hash;
}

// Each cell's level with MCGrid's rules: L bottom layer, H roof regions only beside H, M the rest;
// some roof regions are M (seeded from the heights, so a terrain always gets the same roofs).
std::vector<uint8_t> MCHeightmap::_cell_levels(
		const mc::CellBlock &p_block,
		const uint8_t *data_ptr
) const {
	std::vector<uint8_t> corners(static_cast<size_t>(p_block.corner_count()));
	for (int z = 0; z <= p_block.z; z++) {
		for (int y = 0; y <= p_block.y; y++) {
			for (int x = 0; x <= p_block.x; x++) {
				corners[static_cast<size_t>(p_block.corner_index(x, y, z))] = _is_corner_active(x, y, z, data_ptr) ? 1 : 0;
			}
		}
	}
	bool have_l = tile_library->has_level('l');
	bool have_h = tile_library->has_level('h');
	std::array<bool, 256> m_shapes = {};
	for (int h = 1; h < 255; h++) {
		m_shapes[static_cast<size_t>(h)] = tile_library->get_tile(static_cast<uint8_t>(h)).mesh.is_valid();
	}
	uint32_t seed = 2166136261u; // FNV-1a of the heights
	for (int i = 0; i < heightmap_data.size(); i++) {
		seed = (seed ^ data_ptr[i]) * 16777619u;
	}
	std::vector<uint8_t> roof_m = mc::choose_roof_m(p_block, corners, have_l, have_h, m_shapes, 0.5f, seed);
	return mc::assign_levels(p_block, corners, have_l, have_h, &roof_m);
}

// Spawn the tile of one cell's level, scaled to cell_spacing / height_scale.
void MCHeightmap::_spawn_cell_mesh(
		int x,
		int y,
		int z,
		uint8_t hash,
		uint8_t level
) {
	const MCTile &conf = tile_library->get_tile_for_level(mc::LEVEL_LETTERS[level], hash);
	if (conf.mesh.is_null()) {
		return;
	}

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(conf.mesh_for_cell(Vector3i(x, y, z)));

	// Cell centre in world units, then the tile scaled to the cell size. The origin is scaled
	// exactly once (it used to be scaled again by a scale transform in front, leaving gaps).
	Vector3 scale(cell_spacing, cell_spacing * height_scale, cell_spacing);
	Vector3 pos(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f);
	pos *= scale;

	mi->set_transform(Transform3D(Basis().scaled(scale), pos) * conf.turn);
	add_child(mi); // generated: not owned by the scene, so it is never saved into the .tscn
}

// Small spheres at every column height (debug).
void MCHeightmap::_spawn_debug_spheres(const uint8_t *data_ptr) {
	if (!debug_spheres_container) {
		debug_spheres_container = memnew(Node3D);
		debug_spheres_container->set_name("DebugSpheres");
		add_child(debug_spheres_container);
	}

	Ref<SphereMesh> sphere_mesh;
	sphere_mesh.instantiate();
	sphere_mesh->set_radius(0.1f * cell_spacing);
	sphere_mesh->set_height(0.2f * cell_spacing);

	Ref<StandardMaterial3D> debug_mat;
	debug_mat.instantiate();
	debug_mat->set_albedo(Color(1.0f, 0.0f, 0.0f));

	for (int z = 0; z < grid_dims.y; z++) {
		for (int x = 0; x < grid_dims.x; x++) {
			uint8_t h = data_ptr[x + z * grid_dims.x];
			Vector3 pos(static_cast<float>(x), static_cast<float>(h), static_cast<float>(z));
			pos.x *= cell_spacing;
			pos.y *= cell_spacing * height_scale;
			pos.z *= cell_spacing;

			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(sphere_mesh);
			mi->set_material_override(debug_mat);
			mi->set_position(pos);
			debug_spheres_container->add_child(mi);
		}
	}
}

// Fill heightmap_data from the noise source (or a new random noise).
void MCHeightmap::initialize_random_noise() {
	int required_size = grid_dims.x * grid_dims.y;
	heightmap_data.resize(required_size);
	uint8_t *ptrw = heightmap_data.ptrw();

	if (noise_source.is_valid()) {
		for (int z = 0; z < grid_dims.y; z++) {
			for (int x = 0; x < grid_dims.x; x++) {
				float n = noise_source->get_noise_2d(static_cast<float>(x), static_cast<float>(z));
				float normalized = (n + 1.0f) * 0.5f;
				ptrw[x + z * grid_dims.x] = static_cast<uint8_t>(normalized * 15.0f + 1.0f);
			}
		}
		return;
	}

	auto noise2d = [](float x, float z) {
		unsigned int h =
				static_cast<unsigned int>(floor(x)) * 374761393 + static_cast<unsigned int>(floor(z)) * 668265263;
		h = (h ^ (h >> 13)) * 12741261;
		return static_cast<float>(h & 0x7fffffff) / 2147483647.0f;
	};

	auto fbm = [&](float x, float z) {
		float val = 0.0f;
		float amp = 1.0f;
		float freq = 0.02f;
		for (int i = 0; i < 4; i++) {
			val += noise2d(x * freq, z * freq) * amp;
			amp *= 0.5f;
			freq *= 2.0f;
		}
		return val;
	};

	for (int z = 0; z < grid_dims.y; z++) {
		for (int x = 0; x < grid_dims.x; x++) {
			float n = fbm(static_cast<float>(x), static_cast<float>(z));
			ptrw[x + z * grid_dims.x] = static_cast<uint8_t>(n * 10.0f + 1.0f);
		}
	}
}

// Outside the editor: make random heights if heightmap_data is too small, then generate.
void MCHeightmap::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	int required_size = grid_dims.x * grid_dims.y;
	if (heightmap_data.size() < required_size) {
		initialize_random_noise();
	}
	generate_terrain();
}

} // namespace godot
