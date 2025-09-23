#include "minecraft.h"

#include "godot_cpp/classes/node.hpp"
#include <godot_cpp/classes/engine.hpp>

#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/viewport.hpp>

using namespace godot;

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

	// if (terrain_dirty) {
	// 	generate_smooth_part_mesh("HTerrain", Vector3(0, 0, 0), false);

	// 	terrain_dirty = false;
	// }

	Vector2i camera_part_pos(
			floor(camera->get_global_position().x / part_size),
			floor(camera->get_global_position().z / part_size));

	// Mark all existing parts as inactive for this frame
	for (auto &pair : m_parts) {
		if (pair.second.node) {
			pair.second.node->set_visible(false);
		}
	}

	// Iterate over all parts that should be visible
	for (int z = -render_distance; z <= render_distance; ++z) {
		for (int x = -render_distance; x <= render_distance; ++x) {
			Vector2i part_pos = camera_part_pos + Vector2i(x, z);

			if (m_parts.find(part_pos) == m_parts.end()) {
				// This part is not loaded, so generate it
				Node3D *new_node = generate_part_at(part_pos);
				if (new_node) {
					add_child(new_node);
					m_parts[part_pos] = { new_node };
				}
			} else {
				// This part already exists, just make it visible
				if (m_parts[part_pos].node) {
					m_parts[part_pos].node->set_visible(true);
				}
			}
		}
	}
}

Node3D *MinecraftNode::generate_part_at(Vector2i part_pos) {
	// This function creates and returns a single terrain part.
	// It replaces the old logic that was in _process.
	String name = "Part_" + String::num_int64(part_pos.x) + "_" + String::num_int64(part_pos.y);
	Vector3 world_pos = Vector3(part_pos.x * part_size, 0, part_pos.y * part_size);

	// For now, we will just call the smooth mesh generator.
	// You can add more complex logic here later.
	generate_smooth_part_mesh(name, world_pos, false);

	return Object::cast_to<Node3D>(get_node_or_null(name));
}

void MinecraftNode::_ready() {
	camera = get_viewport()->get_camera_3d();
	if (!camera) {
		UtilityFunctions::print("No active camera found!");
		return;
	}

	setup_ui();

	// minHeapTest();
}
