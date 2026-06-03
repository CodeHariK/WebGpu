/**
 * @file mc_heightmap.cpp
 * @brief Node for generating 2.5D Marching Squares terrain using Marching Cubes meshes.
 * Module Path: src/marching_cubes/mc_heightmap.cpp
 * Build Dependencies: godot-cpp, mc_heightmap.h, mc.h
 */

#include "marching_cubes/mc_heightmap.h"
#include "marching_cubes/mc.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void MCHeightmap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize_random_noise"), &MCHeightmap::initialize_random_noise);
	ClassDB::bind_method(D_METHOD("initialize_random_noise_and_generate"), &MCHeightmap::initialize_random_noise_and_generate);

	ClassDB::bind_method(D_METHOD("set_heightmap_data", "data"), &MCHeightmap::set_heightmap_data);
	ClassDB::bind_method(D_METHOD("get_heightmap_data"), &MCHeightmap::get_heightmap_data);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "heightmap_data"), "set_heightmap_data", "get_heightmap_data");

	ClassDB::bind_method(D_METHOD("set_mc_node", "node"), &MCHeightmap::set_mc_node);
	ClassDB::bind_method(D_METHOD("get_mc_node"), &MCHeightmap::get_mc_node);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mc_node", PROPERTY_HINT_NODE_TYPE, "MCNode"), "set_mc_node", "get_mc_node");

	ClassDB::bind_method(D_METHOD("set_cell_spacing", "spacing"), &MCHeightmap::set_cell_spacing);
	ClassDB::bind_method(D_METHOD("get_cell_spacing"), &MCHeightmap::get_cell_spacing);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_spacing"), "set_cell_spacing", "get_cell_spacing");

	ClassDB::bind_method(D_METHOD("set_height_scale", "scale"), &MCHeightmap::set_height_scale);
	ClassDB::bind_method(D_METHOD("get_height_scale"), &MCHeightmap::get_height_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_scale"), "set_height_scale", "get_height_scale");

	ClassDB::bind_method(D_METHOD("set_grid_size", "size"), &MCHeightmap::set_grid_size);
	ClassDB::bind_method(D_METHOD("get_grid_size"), &MCHeightmap::get_grid_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "grid_size"), "set_grid_size", "get_grid_size");

	ClassDB::bind_method(D_METHOD("set_show_debug_spheres", "show"), &MCHeightmap::set_show_debug_spheres);
	ClassDB::bind_method(D_METHOD("get_show_debug_spheres"), &MCHeightmap::get_show_debug_spheres);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_debug_spheres"), "set_show_debug_spheres", "get_show_debug_spheres");

	ClassDB::bind_method(D_METHOD("set_noise_source", "noise"), &MCHeightmap::set_noise_source);
	ClassDB::bind_method(D_METHOD("get_noise_source"), &MCHeightmap::get_noise_source);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise_source", PROPERTY_HINT_RESOURCE_TYPE, "Noise"), "set_noise_source", "get_noise_source");

	ClassDB::bind_method(D_METHOD("generate_terrain"), &MCHeightmap::generate_terrain);
	ClassDB::bind_method(D_METHOD("clear_terrain"), &MCHeightmap::clear_terrain);
}

MCHeightmap::MCHeightmap() = default;

MCHeightmap::~MCHeightmap() {
	clear_terrain();
}

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

void MCHeightmap::initialize_random_noise_and_generate() {
	initialize_random_noise();
	generate_terrain();
}

void MCHeightmap::generate_terrain() {
	clear_terrain();
	int required_size = grid_size.x * grid_size.y;
	if (heightmap_data.size() < required_size) {
		UtilityFunctions::print("MCHeightmap: Heightmap data size must be at least ", required_size, " bytes.");
		return;
	}
	if (!mc_node) {
		UtilityFunctions::print("MCHeightmap: MCNode reference is not set.");
		return;
	}

	const uint8_t *data = heightmap_data.ptr();
	for (int z = 0; z < grid_size.y - 1; z++) {
		for (int x = 0; x < grid_size.x - 1; x++) {
			uint8_t h00 = data[x + z * grid_size.x];
			uint8_t h10 = data[(x + 1) + z * grid_size.x];
			uint8_t h11 = data[(x + 1) + (z + 1) * grid_size.x];
			uint8_t h01 = data[x + (z + 1) * grid_size.x];

			uint8_t h_min = h00;
			if (h10 < h_min) h_min = h10;
			if (h11 < h_min) h_min = h11;
			if (h01 < h_min) h_min = h01;

			uint8_t h_max = h00;
			if (h10 > h_max) h_max = h10;
			if (h11 > h_max) h_max = h11;
			if (h01 > h_max) h_max = h01;

			int start_y = (h_min > 0) ? (h_min - 1) : 0;
			int end_y = (h_max < 255) ? h_max : 254;

			for (int y = start_y; y <= end_y; y++) {
				uint8_t hash = _get_cell_hash(x, y, z, data);
				if (hash > 0 && hash < 255) {
					_spawn_cell_mesh(x, y, z, hash);
				}
			}
		}
	}

	if (show_debug_spheres) {
		_spawn_debug_spheres(data);
	}
}

void MCHeightmap::clear_terrain() {
	_clear_terrain_meshes();
}

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

bool MCHeightmap::_is_corner_active(int x, int y, int z, const uint8_t *data_ptr) const {
	uint8_t height = data_ptr[x + z * grid_size.x];
	return y <= height;
}

uint8_t MCHeightmap::_get_cell_hash(int x, int y, int z, const uint8_t *data_ptr) const {
	uint8_t hash = 0;
	if (_is_corner_active(x, y, z + 1, data_ptr)) hash |= (1 << 0);
	if (_is_corner_active(x + 1, y, z + 1, data_ptr)) hash |= (1 << 1);
	if (_is_corner_active(x + 1, y, z, data_ptr)) hash |= (1 << 2);
	if (_is_corner_active(x, y, z, data_ptr)) hash |= (1 << 3);
	if (_is_corner_active(x, y + 1, z + 1, data_ptr)) hash |= (1 << 4);
	if (_is_corner_active(x + 1, y + 1, z + 1, data_ptr)) hash |= (1 << 5);
	if (_is_corner_active(x + 1, y + 1, z, data_ptr)) hash |= (1 << 6);
	if (_is_corner_active(x, y + 1, z, data_ptr)) hash |= (1 << 7);
	return hash;
}

void MCHeightmap::_spawn_cell_mesh(int x, int y, int z, uint8_t hash) {
	MeshConfig conf = mc_node->get_mesh_config(hash);
	if (conf.mesh.is_null()) {
		return;
	}

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(conf.mesh);

	Vector3 pos(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f);
	pos.x *= cell_spacing;
	pos.y *= cell_spacing * height_scale;
	pos.z *= cell_spacing;

	Transform3D cell_t;
	cell_t.origin = pos;

	Vector3 scale(cell_spacing, cell_spacing * height_scale, cell_spacing);
	Transform3D scale_t = Transform3D().scaled(scale);

	mi->set_transform(scale_t * cell_t * conf.transform);
	add_child(mi);
	if (is_inside_tree()) {
		mi->set_owner(get_owner() ? get_owner() : this);
	}
}

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

	for (int z = 0; z < grid_size.y; z++) {
		for (int x = 0; x < grid_size.x; x++) {
			uint8_t h = data_ptr[x + z * grid_size.x];
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

void MCHeightmap::initialize_random_noise() {
	int required_size = grid_size.x * grid_size.y;
	heightmap_data.resize(required_size);
	uint8_t *ptrw = heightmap_data.ptrw();

	if (noise_source.is_valid()) {
		for (int z = 0; z < grid_size.y; z++) {
			for (int x = 0; x < grid_size.x; x++) {
				float n = noise_source->get_noise_2d(static_cast<float>(x), static_cast<float>(z));
				float normalized = (n + 1.0f) * 0.5f;
				ptrw[x + z * grid_size.x] = static_cast<uint8_t>(normalized * 15.0f + 1.0f);
			}
		}
		return;
	}

	auto noise2d = [](float x, float z) {
		unsigned int h = static_cast<unsigned int>(floor(x)) * 374761393 + static_cast<unsigned int>(floor(z)) * 668265263;
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

	for (int z = 0; z < grid_size.y; z++) {
		for (int x = 0; x < grid_size.x; x++) {
			float n = fbm(static_cast<float>(x), static_cast<float>(z));
			ptrw[x + z * grid_size.x] = static_cast<uint8_t>(n * 10.0f + 1.0f);
		}
	}
}

void MCHeightmap::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	int required_size = grid_size.x * grid_size.y;
	if (heightmap_data.size() < required_size) {
		initialize_random_noise();
	}
	generate_terrain();
}

} // namespace godot
