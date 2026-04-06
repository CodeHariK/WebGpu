#include "marching_cubes/terrain.h"
#include "marching_cubes/mc.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void MCTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize_terrain", "chunks_x", "chunks_y", "chunks_z", "chunk_size_x", "chunk_size_y", "chunk_size_z"), &MCTerrain::initialize_terrain);
	ClassDB::bind_method(D_METHOD("initialize_terrain_with_noise", "chunks_x", "chunks_y", "chunks_z", "chunk_size_x", "chunk_size_y", "chunk_size_z", "threshold"), &MCTerrain::initialize_terrain_with_noise, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("generate_with_noise"), &MCTerrain::generate_with_noise);
	ClassDB::bind_method(D_METHOD("_ready"), &MCTerrain::_ready);
	ClassDB::bind_method(D_METHOD("_on_noise_changed"), &MCTerrain::_on_noise_changed);

	ClassDB::bind_method(D_METHOD("set_terrain_size", "size"), &MCTerrain::set_terrain_size);
	ClassDB::bind_method(D_METHOD("get_terrain_size"), &MCTerrain::get_terrain_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "terrain_size"), "set_terrain_size", "get_terrain_size");

	ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &MCTerrain::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &MCTerrain::get_chunk_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "chunk_size"), "set_chunk_size", "get_chunk_size");

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &MCTerrain::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &MCTerrain::get_noise);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_noise", "get_noise");

	ClassDB::bind_method(D_METHOD("set_noise_threshold", "threshold"), &MCTerrain::set_noise_threshold);
	ClassDB::bind_method(D_METHOD("get_noise_threshold"), &MCTerrain::get_noise_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_threshold"), "set_noise_threshold", "get_noise_threshold");

	ClassDB::bind_method(D_METHOD("set_trigger_generation", "trigger"), &MCTerrain::set_trigger_generation);
	ClassDB::bind_method(D_METHOD("get_trigger_generation"), &MCTerrain::get_trigger_generation);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trigger_generation"), "set_trigger_generation", "get_trigger_generation");

	ClassDB::bind_method(D_METHOD("set_use_marching_cubes", "use"), &MCTerrain::set_use_marching_cubes);
	ClassDB::bind_method(D_METHOD("get_use_marching_cubes"), &MCTerrain::get_use_marching_cubes);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_marching_cubes"), "set_use_marching_cubes", "get_use_marching_cubes");

	ClassDB::bind_method(D_METHOD("set_mc_node_path", "path"), &MCTerrain::set_mc_node_path);
	ClassDB::bind_method(D_METHOD("get_mc_node_path"), &MCTerrain::get_mc_node_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "mc_node_path"), "set_mc_node_path", "get_mc_node_path");
}

MCTerrain::MCTerrain() {
	noise.instantiate();
	noise->connect("changed", Callable(this, "_on_noise_changed"));
}

MCTerrain::~MCTerrain() {
	if (noise.is_valid()) {
		noise->disconnect("changed", Callable(this, "_on_noise_changed"));
	}
}

void MCTerrain::set_terrain_size(const Vector3i &p_size) {
	if (terrain_size == p_size) {
		return;
	}
	terrain_size = p_size;
	generate_with_noise();
}

void MCTerrain::set_chunk_size(const Vector3i &p_size) {
	if (chunk_size == p_size) {
		return;
	}
	chunk_size = p_size;
	generate_with_noise();
}

void MCTerrain::set_noise(const Ref<FastNoiseLite> &p_noise) {
	if (noise == p_noise) {
		return;
	}

	if (noise.is_valid()) {
		noise->disconnect("changed", Callable(this, "_on_noise_changed"));
	}

	noise = p_noise;

	if (noise.is_valid()) {
		noise->connect("changed", Callable(this, "_on_noise_changed"));
	}

	generate_with_noise();
}

Ref<FastNoiseLite> MCTerrain::get_noise() const {
	return noise;
}

void MCTerrain::set_use_marching_cubes(bool p_use) {
	if (use_marching_cubes == p_use) {
		return;
	}
	use_marching_cubes = p_use;
	generate_with_noise();
}

void MCTerrain::set_mc_node_path(const NodePath &p_path) {
	if (mc_node_path == p_path) {
		return;
	}
	mc_node_path = p_path;
	generate_with_noise();
}

void MCTerrain::set_noise_threshold(float p_threshold) {
	if (Math::is_equal_approx(noise_threshold, p_threshold)) {
		return;
	}
	noise_threshold = p_threshold;
	generate_with_noise();
}

void MCTerrain::set_trigger_generation(bool p_trigger) {
	if (p_trigger) {
		generate_with_noise();
	}
}

void MCTerrain::initialize_terrain(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z) {
	terrain_size = Vector3i(p_chunks_x, p_chunks_y, p_chunks_z);
	chunk_size = Vector3i(p_chunk_size_x, p_chunk_size_y, p_chunk_size_z);

	chunks.clear();
	chunks.resize(static_cast<size_t>(terrain_size.x) * terrain_size.y * terrain_size.z);

	int num_corners = (chunk_size.x + 1) * (chunk_size.y + 1) * (chunk_size.z + 1);
	int num_bytes = (num_corners + 7) / 8;

	UtilityFunctions::print("Initializing terrain: chunks(", p_chunks_x, ", ", p_chunks_y, ", ", p_chunks_z, ") chunk_size(", p_chunk_size_x, ", ", p_chunk_size_y, ", ", p_chunk_size_z, ")");

	for (int z = 0; z < terrain_size.z; z++) {
		for (int y = 0; y < terrain_size.y; y++) {
			for (int x = 0; x < terrain_size.x; x++) {
				int index = x + (y * terrain_size.x) + (z * terrain_size.x * terrain_size.y);
				Chunk &c = chunks[static_cast<size_t>(index)];
				c.size_x = chunk_size.x;
				c.size_y = chunk_size.y;
				c.size_z = chunk_size.z;
				c.loc_x = x;
				c.loc_y = y;
				c.loc_z = z;
				c.corner_states.assign(static_cast<size_t>(num_bytes), 0); // Initialize all bits to 0
			}
		}
	}
}

void MCTerrain::initialize_terrain_with_noise(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z, float p_threshold) {
	_clear_children();

	Ref<BoxMesh> box_mesh;
	box_mesh.instantiate();
	box_mesh->set_size(Vector3(0.2f, 0.2f, 0.2f)); // Slightly bigger for visibility

	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(
			"shader_type spatial;\n"
			"render_mode unshaded;\n" // Added unshaded for better performance
			"void vertex() {}\n"
			"void fragment() {\n"
			"    ALBEDO = vec3(1.0, 1.0, 0.0);\n"
			"}\n");

	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	box_mesh->set_material(material);

	MCNode *mc_node = nullptr;
	if (use_marching_cubes && !mc_node_path.is_empty()) {
		mc_node = Object::cast_to<MCNode>(get_node_or_null(mc_node_path));
	}

	initialize_terrain(p_chunks_x, p_chunks_y, p_chunks_z, p_chunk_size_x, p_chunk_size_y, p_chunk_size_z);

	if (noise.is_null()) {
		UtilityFunctions::print("MCTerrain: Noise resource is null, using default.");
		noise.instantiate();
	}

	int spawn_count = 0;

	for (Chunk &chunk : chunks) {
		_sample_chunk_noise(chunk, noise, p_threshold);
		if (mc_node) {
			spawn_count += _spawn_marching_cubes(chunk, mc_node);
		} else {
			spawn_count += _spawn_debug_cubes(chunk, box_mesh);
		}
	}
	UtilityFunctions::print("Total noise cubes spawned: ", spawn_count);
}

int MCTerrain::_spawn_debug_cubes(const Chunk &p_chunk, const Ref<BoxMesh> &p_box_mesh) {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;
	int count = 0;

	for (int lz = 0; lz < nz; lz++) {
		for (int ly = 0; ly < ny; ly++) {
			for (int lx = 0; lx < nx; lx++) {
				int index = (lz * nx * ny) + (ly * nx) + lx;
				if (!p_chunk.get_corner_bit(index)) {
					continue;
				}

				Vector3 world_pos = Vector3(
						static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx),
						static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly),
						static_cast<float>((p_chunk.loc_z * p_chunk.size_z) + lz));

				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(p_box_mesh);
				mi->set_position(world_pos);
				mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
				add_child(mi);
				if (is_inside_tree()) {
					mi->set_owner(get_owner() ? get_owner() : this);
				}
				count++;
			}
		}
	}
	return count;
}

int MCTerrain::_spawn_marching_cubes(const Chunk &p_chunk, MCNode *p_mc_node) {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;
	int count = 0;

	for (int lz = 0; lz < p_chunk.size_z; lz++) {
		for (int ly = 0; ly < p_chunk.size_y; ly++) {
			for (int lx = 0; lx < p_chunk.size_x; lx++) {
				uint8_t hash = 0;

				// Map 8 corners of the cell to the 8-bit hash
				// Mapping follows the order used in MCNode library generation
				if (p_chunk.get_corner_bit((lz * nx * ny) + (ly * nx) + lx)) {
					hash |= (1 << 7); // C0
				}
				if (p_chunk.get_corner_bit((lz * nx * ny) + (ly * nx) + (lx + 1))) {
					hash |= (1 << 6); // C1
				}
				if (p_chunk.get_corner_bit(((lz + 1) * nx * ny) + (ly * nx) + (lx + 1))) {
					hash |= (1 << 5); // C2
				}
				if (p_chunk.get_corner_bit(((lz + 1) * nx * ny) + (ly * nx) + lx)) {
					hash |= (1 << 4); // C3
				}
				if (p_chunk.get_corner_bit((lz * nx * ny) + ((ly + 1) * nx) + lx)) {
					hash |= (1 << 3); // C4
				}
				if (p_chunk.get_corner_bit((lz * nx * ny) + ((ly + 1) * nx) + (lx + 1))) {
					hash |= (1 << 2); // C5
				}
				if (p_chunk.get_corner_bit(((lz + 1) * nx * ny) + ((ly + 1) * nx) + (lx + 1))) {
					hash |= (1 << 1); // C6
				}
				if (p_chunk.get_corner_bit(((lz + 1) * nx * ny) + ((ly + 1) * nx) + lx)) {
					hash |= (1 << 0); // C7
				}

				if (hash == 0 || hash == 255) {
					continue;
				}

				MeshConfig conf = p_mc_node->get_mesh_config(hash);
				if (conf.mesh.is_null()) {
					continue;
				}

				Vector3 world_pos = Vector3(
						static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx),
						static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly),
						static_cast<float>((p_chunk.loc_z * p_chunk.size_z) + lz));

				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(conf.mesh);
				Transform3D cell_t;
				cell_t.origin = world_pos;
				mi->set_transform(cell_t * conf.transform);
				mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
				add_child(mi);
				if (is_inside_tree()) {
					mi->set_owner(get_owner() ? get_owner() : this);
				}
				count++;
			}
		}
	}
	return count;
}

void MCTerrain::generate_with_noise() {
	initialize_terrain_with_noise(terrain_size.x, terrain_size.y, terrain_size.z, chunk_size.x, chunk_size.y, chunk_size.z, noise_threshold);
}

void MCTerrain::_ready() {
	UtilityFunctions::print("MCTerrain ready, generating...");
	generate_with_noise();
}

void MCTerrain::_clear_children() {
	TypedArray<Node> children = get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (child) {
			child->queue_free();
		}
	}
}

void MCTerrain::_on_noise_changed() {
	generate_with_noise();
}

void MCTerrain::_sample_chunk_noise(Chunk &p_chunk, const Ref<FastNoiseLite> &p_noise, float p_threshold) {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;

	for (int lz = 0; lz < nz; lz++) {
		for (int ly = 0; ly < ny; ly++) {
			for (int lx = 0; lx < nx; lx++) {
				float world_x = static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx);
				float world_y = static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly);
				float world_z = static_cast<float>((p_chunk.loc_z * p_chunk.size_z) + lz);

				float noise_val = p_noise->get_noise_3d(world_x, world_y, world_z);
				int index = (lz * nx * ny) + (ly * nx) + lx;
				p_chunk.set_corner_bit(index, noise_val > p_threshold);
			}
		}
	}
}

} // namespace godot
