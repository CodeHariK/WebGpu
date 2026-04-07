#include "marching_cubes/terrain.h"
#include "marching_cubes/mc.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void MCTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize_terrain", "chunks_x", "chunks_y", "chunks_z", "chunk_size_x", "chunk_size_y", "chunk_size_z"), &MCTerrain::initialize_terrain);
	ClassDB::bind_method(D_METHOD("initialize_terrain_with_noise", "chunks_x", "chunks_y", "chunks_z", "chunk_size_x", "chunk_size_y", "chunk_size_z", "threshold"), &MCTerrain::initialize_terrain_with_noise, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("generate_with_noise"), &MCTerrain::generate_with_noise);

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

	ClassDB::bind_method(D_METHOD("set_trigger_test_grid", "trigger"), &MCTerrain::set_trigger_test_grid);
	ClassDB::bind_method(D_METHOD("get_trigger_test_grid"), &MCTerrain::get_trigger_test_grid);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trigger_test_grid"), "set_trigger_test_grid", "get_trigger_test_grid");

	ClassDB::bind_method(D_METHOD("set_show_debug_corners", "show"), &MCTerrain::set_show_debug_corners);
	ClassDB::bind_method(D_METHOD("get_show_debug_corners"), &MCTerrain::get_show_debug_corners);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_debug_corners"), "set_show_debug_corners", "get_show_debug_corners");

	ClassDB::bind_method(D_METHOD("spawn_test_grid"), &MCTerrain::spawn_test_grid);
	ClassDB::bind_method(D_METHOD("set_mc_node", "node"), &MCTerrain::set_mc_node);
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

void MCTerrain::set_mc_node(MCNode *p_node) {
	mc_node = p_node;
}

void MCTerrain::set_show_debug_corners(bool p_show) {
	if (show_debug_corners == p_show) {
		return;
	}
	show_debug_corners = p_show;
	generate_with_noise();
}

void MCTerrain::set_noise_threshold(float p_threshold) {
	if (Math::is_equal_approx(noise_threshold, p_threshold)) {
		return;
	}
	noise_threshold = p_threshold;
	generate_with_noise();
}

void MCTerrain::set_trigger_test_grid(bool p_trigger) {
	if (p_trigger) {
		spawn_test_grid();
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
	box_mesh->set_size(Vector3(0.2f, 0.2f, 0.2f));

	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(
			"shader_type spatial;\n"
			"render_mode unshaded;\n"
			"void vertex() {}\n"
			"void fragment() {\n"
			"    ALBEDO = vec3(1.0, 1.0, 0.0);\n"
			"}\n");

	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	box_mesh->set_material(material);

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
		}
		if (show_debug_corners) {
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

	Ref<StandardMaterial3D> mat_red;
	mat_red.instantiate();
	mat_red->set_albedo(Color(1, 0, 0));
	mat_red->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	Ref<StandardMaterial3D> mat_white;
	mat_white.instantiate();
	mat_white->set_albedo(Color(1, 1, 1));
	mat_white->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	for (int ly = 0; ly < ny; ly++) {
		for (int lz = 0; lz < nz; lz++) {
			for (int lx = 0; lx < nx; lx++) {
				bool state = p_chunk.get_corner(lx, ly, lz);

				Vector3 world_pos = Vector3(
						static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx),
						static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly),
						static_cast<float>((p_chunk.loc_z * p_chunk.size_z) + lz));

				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(p_box_mesh);
				mi->set_material_override(state ? mat_red : mat_white);
				mi->set_position(world_pos);
				mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
				add_child(mi);
				if (is_inside_tree()) {
					mi->set_owner(get_owner() ? get_owner() : this);
				}
				count++;	total_debug_corners++;
			}
		}
	}
	return count;
}

int MCTerrain::_spawn_marching_cubes(const Chunk &p_chunk, MCNode *p_mc_node) {
	int count = 0;

	for (int ly = 0; ly < p_chunk.size_y; ly++) {
		for (int lz = 0; lz < p_chunk.size_z; lz++) {
			for (int lx = 0; lx < p_chunk.size_x; lx++) {
				uint8_t hash = p_chunk.get_cell_hash(lx, ly, lz);
				total_cells++;

				if (hash == 0) {
					continue;
				}

				MeshConfig conf = p_mc_node->get_mesh_config(hash);
				if (conf.mesh.is_null()) {
					continue;
				}

				// Spawn at center of the 8 corners (cell center)
				Vector3 world_pos = Vector3(
						static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx) + 0.5f,
						static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly) + 0.5f,
						static_cast<float>((p_chunk.loc_z * p_chunk.size_z) + lz) + 0.5f);

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
				count++;	total_mc_meshes++;
			}
		}
	}
	return count;
}

void MCTerrain::generate_with_noise() {
	if (Engine::get_singleton()->is_editor_hint() || !is_inside_tree()) {
		return;
	}
	initialize_terrain_with_noise(terrain_size.x, terrain_size.y, terrain_size.z, chunk_size.x, chunk_size.y, chunk_size.z, noise_threshold);
}

void MCTerrain::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	// Generation is now triggered by MCManager to ensure correct order
	UtilityFunctions::print("MCTerrain: _ready() called, waiting for MCManager...");
}

void MCTerrain::_clear_children() {
	TypedArray<Node> children = get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (child) {
			child->queue_free();
		}
	}
	total_mc_meshes = 0;
	total_debug_corners = 0;
	total_cells = 0;
}

void MCTerrain::_on_noise_changed() {
	generate_with_noise();
}

void MCTerrain::_sample_chunk_noise(Chunk &p_chunk, const Ref<FastNoiseLite> &p_noise, float p_threshold) const {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;

	for (int ly = 0; ly < ny; ly++) {
		for (int lz = 0; lz < nz; lz++) {
			for (int lx = 0; lx < nx; lx++) {
				// Priority 1: Bottommost (Y=0 world-space) is always 1 (solid)
				if (p_chunk.loc_y == 0 && ly == 0) {
					p_chunk.set_corner(lx, ly, lz, true);
					continue;
				}

				// Priority 2: Other world boundaries are always 0 (empty)
				if ((p_chunk.loc_y == terrain_size.y - 1 && ly == p_chunk.size_y) ||
						(p_chunk.loc_x == 0 && lx == 0) ||
						(p_chunk.loc_x == terrain_size.x - 1 && lx == p_chunk.size_x) ||
						(p_chunk.loc_z == 0 && lz == 0) ||
						(p_chunk.loc_z == terrain_size.z - 1 && lz == p_chunk.size_z)) {
					p_chunk.set_corner(lx, ly, lz, false);
					continue;
				}

				// Standard noise sampling for internal corners
				float world_x = static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx);
				float world_y = static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly);
				float world_z = static_cast<float>((p_chunk.loc_z * p_chunk.size_z) + lz);

				float noise_val = p_noise->get_noise_3d(world_x, world_y, world_z);
				p_chunk.set_corner(lx, ly, lz, noise_val > p_threshold);
			}
		}
	}
}

void MCTerrain::spawn_test_grid() {
	_clear_children();

	if (!mc_node) {
		UtilityFunctions::print("MCTerrain: Cannot spawn test grid without valid MCNode.");
		return;
	}

	float spacing = 3.0f;
	Ref<BoxMesh> corner_mesh;
	corner_mesh.instantiate();
	corner_mesh->set_size(Vector3(0.1f, 0.1f, 0.1f));

	Ref<StandardMaterial3D> mat_red;
	mat_red.instantiate();
	mat_red->set_albedo(Color(1, 0, 0));
	mat_red->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	Ref<StandardMaterial3D> mat_white;
	mat_white.instantiate();
	mat_white->set_albedo(Color(1, 1, 1));
	mat_white->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	for (int i = 0; i < 256; i++) {
		int grid_x = i % 16;
		int grid_z = i / 16;
		Vector3 world_origin = Vector3(static_cast<float>(grid_x) * spacing, 0.0f, static_cast<float>(grid_z) * spacing);
		uint8_t hash = static_cast<uint8_t>(i);

		// 1. Spawn Mesh in Center
		MeshConfig conf = mc_node->get_mesh_config(hash);
		if (conf.mesh.is_valid()) {
			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(conf.mesh);
			Transform3D t;
			t.origin = world_origin + Vector3(0.5f, 0.5f, 0.5f);
			mi->set_transform(t * conf.transform);
			add_child(mi);
			if (is_inside_tree()) {
				mi->set_owner(get_owner() ? get_owner() : this);
			}
		}

		// 2. Spawn Corner Debug Cubes
		Vector3 corners[8] = {
			Vector3(0, 0, 1), // C0
			Vector3(1, 0, 1), // C1
			Vector3(1, 0, 0), // C2
			Vector3(0, 0, 0), // C3
			Vector3(0, 1, 1), // C4
			Vector3(1, 1, 1), // C5
			Vector3(1, 1, 0), // C6
			Vector3(0, 1, 0) // C7
		};

		for (int c = 0; c < 8; c++) {
			bool state = (hash & (1 << (7 - c))) != 0;
			MeshInstance3D *c_mi = memnew(MeshInstance3D);
			c_mi->set_mesh(corner_mesh);
			c_mi->set_material_override(state ? mat_red : mat_white);
			c_mi->set_position(world_origin + corners[c]);
			add_child(c_mi);
			if (is_inside_tree()) {
				c_mi->set_owner(get_owner() ? get_owner() : this);
			}
		}

		// 3. Spawn Binary Hash Label
		Label3D *label = memnew(Label3D);
		String binary_str = "";
		for (int b = 7; b >= 0; b--) {
			binary_str += (hash & (1 << b)) ? "1" : "0";
		}
		label->set_text(binary_str);
		label->set_position(world_origin + Vector3(0.5f, 1.3f, 0.5f));
		label->set_font_size(32);
		label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		add_child(label);
		if (is_inside_tree()) {
			label->set_owner(get_owner() ? get_owner() : this);
		}
	}
	UtilityFunctions::print("MCTerrain: Test grid spawned.");
}

} // namespace godot
