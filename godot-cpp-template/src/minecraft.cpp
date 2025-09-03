
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

using namespace godot;

MinecraftNode::MinecraftNode() {
	set_process_mode(Node::ProcessMode::PROCESS_MODE_ALWAYS);
}

MinecraftNode::~MinecraftNode() {
}

void MinecraftNode::_process(double delta) {
	if (terrain_dirty) {
		generate_terrain();
		terrain_dirty = false;
	}
}

void MinecraftNode::_ready() {
}

void MinecraftNode::generate_terrain() {
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

	UtilityFunctions::print("Generated Minecraft!");
}
