#include "minecraft.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <godot_cpp/classes/viewport.hpp>

#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/world3d.hpp>

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>

#include <godot_cpp/classes/file_access.hpp>

#include <godot_cpp/classes/box_mesh.hpp>

#include "unordered_map"

#include "../include/celebi_parse.hpp"
#include "../include/json.hpp"
#include "../include/min_heap.hpp"

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
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (terrain_dirty) {
		String children = String::num_int64(get_child_count());
		for (int i = 0; i < get_child_count(); i++) {
			Node *c = get_child(i);
			children += ", " + c->get_name();
		}
		UtilityFunctions::print(children);

		std::vector<std::vector<int>> heights = generate_terrain_heights(terrain_width, terrain_depth, terrain_scale, terrain_height_scale);

		// generate_terrain(Vector3(0, -.1, 0), heights);
		generate_voxel_terrain(Vector3(0, 0, 0), heights);
		generate_voxel_terrain2(Vector3(0, .1, 0), heights);
		terrain_dirty = false;
	}

	// Vector3 origin = camera->get_global_transform().origin;
	// Vector3 direction = -camera->get_global_transform().basis.get_column(2); // forward
	// Vector3 end = origin + direction * 100.0f;

	// Ref<World3D> world = get_world_3d();
	// if (world.is_null())
	// 	return;

	// PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
	// if (!space_state)
	// 	return;

	// Ref<PhysicsRayQueryParameters3D> params = PhysicsRayQueryParameters3D::create(origin, end);
	// Dictionary result = space_state->intersect_ray(params);

	// if (!result.is_empty()) {
	// 	Vector3 hit_pos = result["position"];
	// 	Vector3 hit_normal = result["normal"];
	// 	Object *collider = Object::cast_to<Object>(result["collider"]);

	// 	UtilityFunctions::print("Hit at: ", hit_pos);
	// 	UtilityFunctions::print("Normal: ", hit_normal);

	// 	if (Node3D *node = Object::cast_to<Node3D>(collider)) {
	// 		UtilityFunctions::print("Hit node: ", node->get_name());
	// 		if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(node)) {
	// 			Ref<Material> mat = mi->get_active_material(0);
	// 			if (mat.is_valid()) {
	// 				UtilityFunctions::print("Material: ", mat->get_name());
	// 			}
	// 		}
	// 	}
	// } else {
	// 	UtilityFunctions::print("No hit");
	// }
}

void MinecraftNode::_ready() {
	camera = get_viewport()->get_camera_3d();
	if (!camera) {
		UtilityFunctions::print("No active camera found!");
	}
	UtilityFunctions::print(camera);
}

std::vector<std::vector<int>> MinecraftNode::generate_terrain_heights(int width, int depth, float scale, float height_scale) {
	std::vector<std::vector<int>> heights(width, std::vector<int>(depth));

	Ref<FastNoiseLite> noise;
	noise.instantiate();
	noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
	noise->set_seed(1337);
	noise->set_frequency(scale);

	for (int z = 0; z < depth; ++z) {
		for (int x = 0; x < width; ++x) {
			float h = noise->get_noise_2d((float)x + 0.5f, (float)z + 0.5f) * height_scale;
			heights[x][z] = (int)Math::floor(h);
		}
	}

	return heights;
}

void MinecraftNode::generate_terrain(Vector3 pos, std::vector<std::vector<int>> heights) {
	PackedVector3Array vertices;
	PackedInt32Array indices;

	// Generate vertices from height map
	for (int z = 0; z < terrain_depth; ++z) {
		for (int x = 0; x < terrain_width; ++x) {
			float y = heights[x][z];
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
	// mi->set_material_override(terrain_material);
}

void MinecraftNode::generate_voxel_terrain(Vector3 pos, std::vector<std::vector<int>> heights) {
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

	// Create a shared cube mesh
	Ref<BoxMesh> cube_mesh;
	cube_mesh.instantiate();
	cube_mesh->set_size(Vector3(1, 1, 1));

	// Step 2: Place cubes for top + exposed sides
	for (int z = 0; z < terrain_depth; ++z) {
		for (int x = 0; x < terrain_width; ++x) {
			int h = heights[x][z];

			// --- Top cube
			{
				MeshInstance3D *cube = memnew(MeshInstance3D);
				cube->set_mesh(cube_mesh);
				// cube->set_material_override(terrain_material);
				cube->set_position(Vector3(pos.x + x + 0.5f, pos.y + h + 0.5f, pos.z + z + 0.5f));
				cube->set_scale(Vector3(0.9f, 0.9f, 0.9f));
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
						// cube->set_material_override(terrain_material);
						cube->set_position(Vector3(pos.x + x + 0.5f, pos.y + y + 0.5f, pos.z + z + 0.5f));
						cube->set_scale(Vector3(0.9f, 0.9f, 0.9f));
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

static std::array<Vector3, 4> get_face_vertices(const Vector3 &pos, const Vector3 &dir) {
	if (dir == Vector3(0, 1, 0)) { // top face (Y+)
		return {
			pos + Vector3(0, 1, 0), // back-left
			pos + Vector3(1, 1, 0), // back-right
			pos + Vector3(1, 1, 1), // front-right
			pos + Vector3(0, 1, 1) // front-left
		};
	}
	if (dir == Vector3(0, -1, 0)) { // bottom face (Y-)
		return {
			pos + Vector3(0, 0, 0), // back-left
			pos + Vector3(0, 0, 1), // front-left
			pos + Vector3(1, 0, 1), // front-right
			pos + Vector3(1, 0, 0) // back-right
		};
	}
	if (dir == Vector3(1, 0, 0)) { // right face (+X)
		return {
			pos + Vector3(1, 0, 0), // back-bottom
			pos + Vector3(1, 1, 0), // back-top
			pos + Vector3(1, 1, 1), // front-top
			pos + Vector3(1, 0, 1) // front-bottom
		};
	}
	if (dir == Vector3(-1, 0, 0)) { // left face (-X)
		return {
			pos + Vector3(0, 0, 1), // front-bottom
			pos + Vector3(0, 1, 1), // front-top
			pos + Vector3(0, 1, 0), // back-top
			pos + Vector3(0, 0, 0) // back-bottom
		};
	}
	if (dir == Vector3(0, 0, 1)) { // front face (+Z)
		return {
			pos + Vector3(0, 0, 1), // left-bottom
			pos + Vector3(0, 1, 1), // left-top
			pos + Vector3(1, 1, 1), // right-top
			pos + Vector3(1, 0, 1) // right-bottom
		};
	}
	// back face (-Z)
	return {
		pos + Vector3(1, 0, 0), // right-bottom
		pos + Vector3(1, 1, 0), // right-top
		pos + Vector3(0, 1, 0), // left-top
		pos + Vector3(0, 0, 0) // left-bottom
	};
}

// Add this helper function to calculate UV coordinates
// Update the tile size constant and UV calculation
static Vector2 get_atlas_uv(int tile_x, int tile_y) {
	const float TILE_SIZE = 0.125f; // 1/8 since it's 8x8 grid
	return Vector2(tile_x * TILE_SIZE, tile_y * TILE_SIZE);
}

void add_face_uvs(PackedVector2Array &uvs, const Vector3 &dir) {
	uvs.push_back(Vector2(0, 0));
	uvs.push_back(Vector2(0, 0.125));
	uvs.push_back(Vector2(0.125, 0.125));
	uvs.push_back(Vector2(0.125, 0));
	return;

	Vector2 uv_base;
	const float TILE_SIZE = 0.125f; // 1/8

	// Map each face to different tiles in 8x8 grid
	if (dir == Vector3(0, 1, 0))
		uv_base = get_atlas_uv(0, 0); // top
	else if (dir == Vector3(0, -1, 0))
		uv_base = get_atlas_uv(1, 0); // bottom
	else if (dir == Vector3(1, 0, 0))
		uv_base = get_atlas_uv(2, 0); // right
	else if (dir == Vector3(-1, 0, 0))
		uv_base = get_atlas_uv(3, 0); // left
	else if (dir == Vector3(0, 0, 1))
		uv_base = get_atlas_uv(4, 0); // front
	else
		uv_base = get_atlas_uv(5, 0); // back

	// Add UVs in counter-clockwise order
	uvs.push_back(uv_base + Vector2(0, 0)); // bottom-left
	uvs.push_back(uv_base + Vector2(0, TILE_SIZE)); // top-left
	uvs.push_back(uv_base + Vector2(TILE_SIZE, TILE_SIZE)); // top-right
	uvs.push_back(uv_base + Vector2(TILE_SIZE, 0)); // bottom-right
}

// Add these structures at file scope
struct FaceData {
	std::array<Vector3, 4> vertices;
	std::array<Vector2, 4> uvs;
};

// Add this before the face_lookup definition
struct Vector3Hasher {
	std::size_t operator()(const godot::Vector3 &v) const {
		std::size_t h1 = std::hash<float>{}(v.x);
		std::size_t h2 = std::hash<float>{}(v.y);
		std::size_t h3 = std::hash<float>{}(v.z);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

// Then modify the face_lookup declaration
static const std::unordered_map<godot::Vector3, FaceData, Vector3Hasher> face_lookup = {
	{ Vector3(0, 1, 0), { // top face (Y+)
								{ {
										Vector3(0, 1, 0), // back-left
										Vector3(1, 1, 0), // back-right
										Vector3(1, 1, 1), // front-right
										Vector3(0, 1, 1) // front-left
								} },
								{ { Vector2(0, 0), Vector2(0.125, 0), Vector2(0.125, 0.125), Vector2(0, 0.125) } } } },
	{ Vector3(0, -1, 0), { // bottom face (Y-)
								 { { Vector3(0, 0, 0), Vector3(0, 0, 1), Vector3(1, 0, 1), Vector3(1, 0, 0) } }, { { Vector2(0.125, 0), Vector2(0.125, 0.125), Vector2(0.25, 0.125), Vector2(0.25, 0) } } } },
	{ Vector3(1, 0, 0), { // right face (+X)
								{ { Vector3(1, 0, 0), Vector3(1, 1, 0), Vector3(1, 1, 1), Vector3(1, 0, 1) } }, { { Vector2(0.25, 0), Vector2(0.25, 0.125), Vector2(0.375, 0.125), Vector2(0.375, 0) } } } },
	{ Vector3(-1, 0, 0), { // left face (-X)
								 { { Vector3(0, 0, 0), Vector3(0, 1, 0), Vector3(0, 1, 1), Vector3(0, 0, 1) } }, { { Vector2(0.375, 0), Vector2(0.375, 0.125), Vector2(0.5, 0.125), Vector2(0.5, 0) } } } },
	{ Vector3(0, 0, 1), { // front face (+Z)
								{ { Vector3(0, 0, 1), Vector3(0, 1, 1), Vector3(1, 1, 1), Vector3(1, 0, 1) } }, { { Vector2(0.5, 0), Vector2(0.5, 0.125), Vector2(0.625, 0.125), Vector2(0.625, 0) } } } },
	{ Vector3(0, 0, -1), { // back face (-Z)
								 { { Vector3(1, 0, 0), Vector3(1, 1, 0), Vector3(0, 1, 0), Vector3(0, 0, 0) } }, { { Vector2(0.625, 0), Vector2(0.625, 0.125), Vector2(0.75, 0.125), Vector2(0.75, 0) } } } }
};

// Then modify build_chunk_mesh to use the lookup map:
void add_face(PackedVector3Array &vertices, PackedVector3Array &normals,
		PackedVector2Array &uvs, PackedInt32Array &indices,
		const Vector3 &pos, const Vector3 &dir, int &index_offset) {
	const auto &face_data = face_lookup.at(dir);

	// Add vertices
	for (const auto &v : face_data.vertices) {
		vertices.push_back(pos + v);
		normals.push_back(dir);
	}

	// Add UVs
	for (const auto &uv : face_data.uvs) {
		uvs.push_back(uv);
	}

	// Add indices
	indices.push_back(index_offset + 0);
	indices.push_back(index_offset + 1);
	indices.push_back(index_offset + 2);
	indices.push_back(index_offset + 2);
	indices.push_back(index_offset + 3);
	indices.push_back(index_offset + 0);

	index_offset += 4;
}

// Ref<ArrayMesh> build_chunk_mesh(const std::vector<std::vector<int>> &heights, int size) {
// 	PackedVector3Array vertices;
// 	PackedVector3Array normals;
// 	PackedVector2Array uvs;
// 	PackedInt32Array indices;
// 	int index_offset = 0;

// 	int width = int(heights.size());
// 	int depth = int(heights[0].size());

// 	for (int x = 0; x < width; x++) {
// 		for (int z = 0; z < depth; z++) {
// 			int h = heights[x][z];
// 			// Top face
// 			{
// 				Vector3 pos(x, h, z);
// 				Vector3 dir(0, 1, 0);
// 				auto face_verts = get_face_vertices(pos, dir);
// 				for (auto &v : face_verts) {
// 					vertices.push_back(v);
// 					normals.push_back(dir);
// 				}
// 				add_face_uvs(uvs, dir); // Use the helper function
// 				indices.push_back(index_offset + 0);
// 				indices.push_back(index_offset + 1);
// 				indices.push_back(index_offset + 2);
// 				indices.push_back(index_offset + 2);
// 				indices.push_back(index_offset + 3);
// 				indices.push_back(index_offset + 0);
// 				index_offset += 4;
// 			}

// 			// Sides
// 			const int dx[4] = { -1, 1, 0, 0 };
// 			const int dz[4] = { 0, 0, -1, 1 };
// 			const Vector3 dirs[4] = { Vector3(-1, 0, 0), Vector3(1, 0, 0), Vector3(0, 0, -1), Vector3(0, 0, 1) };
// 			// for (int dir_idx = 0; dir_idx < 4; dir_idx++) {
// 			for (int dir_idx = 0; dir_idx < 4; dir_idx++) {
// 				int nx = x + dx[dir_idx];
// 				int nz = z + dz[dir_idx];
// 				int neighbor_h = 0;
// 				if (nx >= 0 && nx < width && nz >= 0 && nz < depth) {
// 					neighbor_h = heights[nx][nz];
// 				}
// 				if (h > neighbor_h) {
// 					for (int y = neighbor_h + 1; y <= h; y++) {
// 						Vector3 pos(x, y, z);
// 						Vector3 dir = dirs[dir_idx];
// 						auto face_verts = get_face_vertices(pos, dir);
// 						for (auto &v : face_verts) {
// 							vertices.push_back(v);
// 							normals.push_back(dir);
// 						}
// 						add_face_uvs(uvs, dir); // Use the helper function
// 						indices.push_back(index_offset + 0);
// 						indices.push_back(index_offset + 1);
// 						indices.push_back(index_offset + 2);
// 						indices.push_back(index_offset + 2);
// 						indices.push_back(index_offset + 3);
// 						indices.push_back(index_offset + 0);
// 						index_offset += 4;
// 					}
// 				}
// 			}
// 		}
// 	}

// 	Array arrays;
// 	arrays.resize(Mesh::ARRAY_MAX);
// 	arrays[Mesh::ARRAY_VERTEX] = vertices;
// 	arrays[Mesh::ARRAY_NORMAL] = normals;
// 	arrays[Mesh::ARRAY_TEX_UV] = uvs;
// 	arrays[Mesh::ARRAY_INDEX] = indices;

// 	Ref<ArrayMesh> mesh;
// 	mesh.instantiate();
// 	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
// 	return mesh;
// }

Ref<ArrayMesh> build_chunk_mesh(const std::vector<std::vector<int>> &heights, int size) {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedInt32Array indices;
	int index_offset = 0;

	int width = int(heights.size());
	int depth = int(heights[0].size());

	for (int x = 0; x < width; x++) {
		for (int z = 0; z < depth; z++) {
			int h = heights[x][z];

			// Top face
			add_face(vertices, normals, uvs, indices,
					Vector3(x, h, z), Vector3(0, 1, 0), index_offset);

			// Side faces
			const Vector3 dirs[4] = {
				Vector3(-1, 0, 0), Vector3(1, 0, 0),
				Vector3(0, 0, -1), Vector3(0, 0, 1)
			};
			const int dx[4] = { -1, 1, 0, 0 };
			const int dz[4] = { 0, 0, -1, 1 };

			for (int dir_idx = 0; dir_idx < 4; dir_idx++) {
				int nx = x + dx[dir_idx];
				int nz = z + dz[dir_idx];
				int neighbor_h = 0;
				if (nx >= 0 && nx < width && nz >= 0 && nz < depth) {
					neighbor_h = heights[nx][nz];
				}
				if (h > neighbor_h) {
					for (int y = neighbor_h + 1; y <= h; y++) {
						add_face(vertices, normals, uvs, indices,
								Vector3(x, y - 1, z), dirs[dir_idx], index_offset);
					}
				}
			}
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

void MinecraftNode::generate_voxel_terrain2(Vector3 pos, std::vector<std::vector<int>> heights) {
	// Create voxel data
	int size = terrain_width;

	// Build one mesh directly from heights
	Ref<ArrayMesh> mesh = build_chunk_mesh(heights, size);

	// Place a MeshInstance3D in the scene
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(get_node_or_null("VoxelChunk"));
	if (!mi) {
		mi = memnew(MeshInstance3D);
		mi->set_name("VoxelChunk");
		add_child(mi);
	}
	mi->set_mesh(mesh);
	mi->set_material_override(terrain_material);
	mi->set_position(pos);
}
