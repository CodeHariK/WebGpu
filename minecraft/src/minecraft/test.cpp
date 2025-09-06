#include "minecraft.h"

#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/ref.hpp"

#include "godot_cpp/core/memory.hpp"
#include "godot_cpp/variant/string.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

#include <godot_cpp/classes/file_access.hpp>

#include "../include/celebi_parse.hpp"
#include "../include/json.hpp"
#include "../include/min_heap.hpp"

using njson = nlohmann::json;

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
