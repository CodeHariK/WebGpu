
#include "voxel.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

using namespace godot;

void VoxelNode::_bind_methods() {
}

VoxelNode::VoxelNode() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_process_mode(Node::ProcessMode::PROCESS_MODE_DISABLED);
	}

	blendTest();
}

VoxelNode::~VoxelNode() {
}

void VoxelNode::_process(double delta) {
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
