#include "voxel.h"

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

using namespace godot;

VoxelNode::VoxelNode() {
	set_process_mode(Node::ProcessMode::PROCESS_MODE_ALWAYS);
}

VoxelNode::~VoxelNode() {
}

void VoxelNode::_process(double delta) {
	if (terrain_dirty) {
		generate_terrain();
		terrain_dirty = false;
	}
}

void VoxelNode::_ready() {
}

void VoxelNode::blendTest() {
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

void VoxelNode::make_triangle() {
	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	// 3 vertices
	PackedVector3Array vertices;
	vertices.push_back(Vector3(0, 1, 0));
	vertices.push_back(Vector3(-1, -1, 0));
	vertices.push_back(Vector3(1, -1, 0));

	// Indices for a single triangle
	PackedInt32Array indices;
	indices.push_back(0);
	indices.push_back(1);
	indices.push_back(2);

	// Build surface array
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_INDEX] = indices;

	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	// Put it in the scene
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(mesh);
	mi->set_name("GeneratedTriangle");
	add_child(mi);

	UtilityFunctions::print("Generated triangle mesh!");
}

void VoxelNode::make_quad() {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	st->add_vertex(Vector3(-1, 0, -1));
	st->add_vertex(Vector3(1, 0, -1));
	st->add_vertex(Vector3(1, 0, 1));

	st->add_vertex(Vector3(-1, 0, -1));
	st->add_vertex(Vector3(1, 0, 1));
	st->add_vertex(Vector3(-1, 0, 1));

	Ref<ArrayMesh> mesh = st->commit();

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(mesh);
	mi->set_name("GeneratedQuad");
	add_child(mi);

	UtilityFunctions::print("Generated quad mesh!");
}

void VoxelNode::generate_terrain() {
	for (int i = get_child_count() - 1; i >= 0; --i) {
		Node *child = get_child(i);
		remove_child(child);
		child->queue_free();
	}

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
			vertices.push_back(Vector3((float)x, y, (float)z));
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
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(mesh);
	mi->set_name("NoiseTerrain");
	add_child(mi);

	// Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://shaders/terrain.gdshader");
	// Ref<ShaderMaterial> mat;
	// mat.instantiate();
	// mat->set_shader(shader);
	mi->set_material_override(terrain_material);

	UtilityFunctions::print("Generated noise terrain mesh!");
}
