
#include "minecraft.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <godot_cpp/classes/surface_tool.hpp>

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>

#include <godot_cpp/classes/file_access.hpp>

#include <godot_cpp/classes/box_mesh.hpp>

#include "include/celebi_parse.hpp"
#include "include/json.hpp"
#include "include/min_heap.hpp"


using namespace godot;
using njson = nlohmann::json;

MinecraftNode::MinecraftNode() {
	set_process_mode(Node::ProcessMode::PROCESS_MODE_INHERIT);
}

MinecraftNode::~MinecraftNode() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_process_mode(Node::ProcessMode::PROCESS_MODE_DISABLED);
	}
}

void MinecraftNode::_process(double delta) {
	if (terrain_dirty) {
		String children = String::num_int64(get_child_count());
		for (int i = 0; i < get_child_count(); i++) {
			Node *c = get_child(i);
			children += ", " + c->get_name();
		}
		UtilityFunctions::print(children);

		generate_terrain(Vector3(100, 0, 0));
		generate_voxel_terrain(Vector3(0, 0, 0));
		terrain_dirty = false;
	}
}

void MinecraftNode::_ready() {
}

void MinecraftNode::generate_terrain(Vector3 pos) {
	Ref<FastNoiseLite> noise;
	noise.instantiate();
	noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
	noise->set_seed(1337);
	noise->set_frequency(terrain_scale);

	PackedVector3Array vertices;
	PackedInt32Array indices;

	// Generate vertices
	for (int z = 0; z < terrain_depth; ++z) {
		for (int x = 0; x < terrain_width; ++x) {
			float y = noise->get_noise_2d((float)x, (float)z) * terrain_height_scale;
			vertices.push_back(Vector3(pos.x + (float)x, pos.y + y, pos.z + (float)z));
		}
	}

	// Generate indices (two triangles per quad)
	for (int z = 0; z < terrain_depth - 1; ++z) {
		for (int x = 0; x < terrain_width - 1; ++x) {
			int i0 = x + z * terrain_width;
			int i1 = (x + 1) + z * terrain_width;
			int i2 = x + (z + 1) * terrain_width;
			int i3 = (x + 1) + (z + 1) * terrain_width;
			// First triangle
			// First triangle
			indices.push_back(i0);
			indices.push_back(i1);
			indices.push_back(i2);
			// Second triangle
			indices.push_back(i1);
			indices.push_back(i3);
			indices.push_back(i2);
		}
	}

	PackedVector3Array normals;
	normals.resize(vertices.size());
	for (int i = 0; i < normals.size(); ++i) {
		normals[i] = Vector3(0, 1, 0);
	}

	// Build surface array
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	// Create MeshInstance3D
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(get_node_or_null("NoiseTerrain"));
	if (!mi) {
		mi = memnew(MeshInstance3D);
		mi->set_name("NoiseTerrain");
		add_child(mi);
	}
	mi->set_mesh(mesh);

	// Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://shaders/terrain.gdshader");
	// Ref<ShaderMaterial> mat;
	// mat.instantiate();
	// mat->set_shader(shader);
	mi->set_material_override(terrain_material);
}

void MinecraftNode::generate_voxel_terrain(Vector3 pos) {
	// Parent node to hold all cubes
	Node3D *voxel_parent = Object::cast_to<Node3D>(get_node_or_null("VoxelTerrain"));
	if (!voxel_parent) {
		voxel_parent = memnew(Node3D);
		voxel_parent->set_name("VoxelTerrain");
		add_child(voxel_parent);
	} else {
		// clear children cubes before reusing
		for (int i = voxel_parent->get_child_count() - 1; i >= 0; --i) {
			voxel_parent->get_child(i)->queue_free();
		}
	}

	Ref<FastNoiseLite> noise;
	noise.instantiate();
	noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
	noise->set_seed(1337);
	noise->set_frequency(terrain_scale);

	// Create a shared cube mesh
	Ref<BoxMesh> cube_mesh;
	cube_mesh.instantiate();
	cube_mesh->set_size(Vector3(1, 1, 1));

	// for (int z = 0; z < terrain_depth; ++z) {
	// 	for (int x = 0; x < terrain_width; ++x) {
	// 		// Sample height at center of voxel
	// 		float height = noise->get_noise_2d((float)x + 0.5f, (float)z + 0.5f) * terrain_height_scale;
	// 		int max_y = (int)Math::floor(height);

	// 		MeshInstance3D *cube_instance = memnew(MeshInstance3D);
	// 		cube_instance->set_mesh(cube_mesh);
	// 		cube_instance->set_material_override(terrain_material);
	// 		cube_instance->set_position(Vector3(pos.x + (float)x + 0.5f, pos.y + (float)max_y + 0.5f, pos.z + (float)z + 0.5f));
	// 		voxel_parent->add_child(cube_instance);
	// 	}
	// }

	// Step 1: Precompute heights
	std::vector<std::vector<int>> heights(terrain_width, std::vector<int>(terrain_depth));

	for (int z = 0; z < terrain_depth; ++z) {
		for (int x = 0; x < terrain_width; ++x) {
			float h = noise->get_noise_2d((float)x + 0.5f, (float)z + 0.5f) * terrain_height_scale;
			heights[x][z] = (int)Math::floor(h);
		}
	}

	// Step 2: Place cubes for top + exposed sides
	for (int z = 0; z < terrain_depth; ++z) {
		for (int x = 0; x < terrain_width; ++x) {
			int h = heights[x][z];

			// --- Top cube
			{
				MeshInstance3D *cube = memnew(MeshInstance3D);
				cube->set_mesh(cube_mesh);
				cube->set_material_override(terrain_material);
				cube->set_position(Vector3(pos.x + x + 0.5f, pos.y + h + 0.5f, pos.z + z + 0.5f));
				voxel_parent->add_child(cube);
			}

			// --- Check neighbors
			const int dx[4] = { -1, 1, 0, 0 };
			const int dz[4] = { 0, 0, -1, 1 };

			for (int dir = 0; dir < 4; dir++) {
				int nx = x + dx[dir];
				int nz = z + dz[dir];

				int neighbor_h = 0;
				if (nx >= 0 && nx < terrain_width && nz >= 0 && nz < terrain_depth) {
					neighbor_h = heights[nx][nz];
				}

				// If current column is higher, fill the side wall
				if (h > neighbor_h) {
					for (int y = neighbor_h + 1; y <= h; y++) {
						MeshInstance3D *cube = memnew(MeshInstance3D);
						cube->set_mesh(cube_mesh);
						cube->set_material_override(terrain_material);
						cube->set_position(Vector3(pos.x + x + 0.5f, pos.y + y + 0.5f, pos.z + z + 0.5f));
						voxel_parent->add_child(cube);
					}
				}
			}
		}
	}
}

void MinecraftNode::blendTest() {
	// Load the entire .blend file as a PackedScene and print mesh/object/material names
	Ref<Resource> blend_resource = ResourceLoader::get_singleton()->load("res://assets/Voxel.blend");
	if (blend_resource.is_valid() && blend_resource->is_class("PackedScene")) {
		Ref<PackedScene> packed_scene = blend_resource;
		Node *scene_root = packed_scene->instantiate();
		if (scene_root) {
			// Recursive lambda for traversing children
			int mesh_instance_counter = 0;
			std::function<void(Node *)> traverse = [&](Node *node) {
				if (Object::cast_to<Node3D>(node)) {
					UtilityFunctions::print(String("Node3D: ") + node->get_name());
				}
				MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(node);
				if (mesh_instance) {
					Ref<Mesh> m = mesh_instance->get_mesh();
					if (m.is_valid()) {
						UtilityFunctions::print(String("  Mesh: ") + m->get_name());
						int surf_count = m->get_surface_count();
						for (int i = 0; i < surf_count; ++i) {
							Ref<Material> mat = m->surface_get_material(i);
							if (mat.is_valid()) {
								UtilityFunctions::print(String("    Material: ") + mat->get_name());
							}
						}
						// Instantiate a new MeshInstance3D for each mesh and add it as child to this HelloNode
						MeshInstance3D *new_mesh_instance = memnew(MeshInstance3D);
						new_mesh_instance->set_mesh(m);
						new_mesh_instance->set_name(String("ImportedMeshInstance") + String::num(mesh_instance_counter++));
						new_mesh_instance->set_transform(mesh_instance->get_transform());
						this->add_child(new_mesh_instance);
					}
				}
				int child_count = node->get_child_count();
				for (int i = 0; i < child_count; ++i) {
					Node *child = node->get_child(i);
					traverse(child);
				}
			};
			traverse(scene_root);
			scene_root->queue_free();
		}
	}
}


void MinecraftNode::minHeapTest() {
	MinHeap<std::string> pq;

	pq.insert("apple", 5);
	pq.insert("banana", 2);
	pq.insert("cherry", 8);

	godot::UtilityFunctions::print(String("Peek: ") + pq.peek().value().c_str()); // banana

	if (!pq.updatePriority("apple", 1)) {
		godot::UtilityFunctions::print("Item not found in priority queue");
	}

	for (const auto &node : pq.getElements()) {
		godot::UtilityFunctions::print(String("item=") + node.item.c_str() + " priority=" + String::num_real(node.priority));
	}

	godot::UtilityFunctions::print(String("ExtractMin: ") + pq.extractMin().value().c_str()); // apple
	godot::UtilityFunctions::print(String("ExtractMin: ") + pq.extractMin().value().c_str()); // banana
	godot::UtilityFunctions::print(String("ExtractMin: ") + pq.extractMin().value().c_str()); // cherry
}

void MinecraftNode::jsonTest() {
	std::string planetJsonStr = R"(
		{
			"planets": ["Mercury", "Venus", "Earth", "Mars",
						"Jupiter", "Uranus", "Neptune"]
		}
	)";

	// parse without exceptions
	njson j = njson::parse(planetJsonStr, nullptr, false);

	if (j.is_discarded()) {
		godot::UtilityFunctions::print("Failed to parse JSON!");
	} else {
		godot::UtilityFunctions::print(String("Parsed planets: ") + j.dump(2).c_str());
	}

	// ---- Load and parse library.celebi without exceptions ----
	Ref<FileAccess> f = FileAccess::open("res://data/library.json", FileAccess::READ);
	if (f.is_null()) {
		UtilityFunctions::print("Could not open library.json");
		return;
	}

	String content = f->get_as_text();
	njson libraryJson = njson::parse(std::string(content.utf8().get_data()), nullptr, false);

	if (libraryJson.is_discarded()) {
		UtilityFunctions::print("Parse error: invalid JSON in library.json");
		return;
	}

	Celebi::Data d1 = libraryJson.get<Celebi::Data>();
	UtilityFunctions::print(String("library.json item count: ") + String::num_int64(d1.items.size()));
	for (const auto &item : d1.items) {
		UtilityFunctions::print(String(item.obj.c_str()));
	}
}
