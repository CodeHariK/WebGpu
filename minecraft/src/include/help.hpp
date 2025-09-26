#pragma once

#include "godot_cpp/classes/packed_scene.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/templates/vector.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/string.hpp"
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

using namespace godot;

namespace Help {

inline void traverse(godot::Dictionary &mesh_map, Node *node) {
	MeshInstance3D *meshInstance = Object::cast_to<MeshInstance3D>(node);
	if (meshInstance) {
		Ref<Mesh> meshObj = meshInstance->get_mesh();
		if (meshObj.is_valid()) {
			String name = meshInstance->get_name();
			if (name.begins_with("c_")) {
				name = name.substr(2); // Remove the "c_" prefix.
			}

			meshObj->set_name(name);
			mesh_map[name] = meshObj;
		}
	}
	int child_count = node->get_child_count();
	for (int i = 0; i < child_count; ++i) {
		Node *child = node->get_child(i);
		traverse(mesh_map, child);
	}
}

inline godot::Dictionary loadMeshFile(String path) {
	godot::Dictionary mesh_map;
	Ref<Resource> blend_resource = ResourceLoader::get_singleton()->load(path);
	if (blend_resource.is_valid() && blend_resource->is_class("PackedScene")) {
		Ref<PackedScene> packed_scene = blend_resource;
		Node *scene_root = packed_scene->instantiate();
		if (scene_root) {
			traverse(mesh_map, scene_root);
			scene_root->queue_free();
		}
	}
	return mesh_map;
}

inline String vector_to_string(const Vector<int> &v) {
	String s = "[";
	for (int i = 0; i < v.size(); i++) {
		s += String::num_int64(v[i]); // convert the integer to a String
		if (i < v.size() - 1) {
			s += ", ";
		}
	}
	s += "]";
	return s;
}

} //namespace Help