#include "minecraft.h"

#include <cmath>
#include <cstdint>

#include "godot_cpp/classes/input.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/vector2i.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/viewport.hpp>

using namespace godot;

MinecraftNode::MinecraftNode() {
	set_process_mode(Node::ProcessMode::PROCESS_MODE_INHERIT);
	m_last_camera_part_pos = Vector2i(INT32_MAX, INT32_MAX);
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

	// Toggle wireframe view on key press
	Input *input = Input::get_singleton();
	if (input->is_action_just_pressed("toggle_wireframe")) {
		Viewport *viewport = get_viewport();
		if (viewport->get_debug_draw() == Viewport::DEBUG_DRAW_WIREFRAME) {
			viewport->set_debug_draw(Viewport::DEBUG_DRAW_DISABLED);
		} else {
			viewport->set_debug_draw(Viewport::DEBUG_DRAW_WIREFRAME);
		}
	}

	Vector2i camera_part_pos(
			floor(camera->get_global_position().x / part_size),
			floor(camera->get_global_position().z / part_size));

	if (m_last_camera_part_pos != camera_part_pos) {
		m_last_camera_part_pos = camera_part_pos;

		// Unload parts that are now too far away
		for (auto it = m_parts.begin(); it != m_parts.end();) {
			if (abs(it->first.x - camera_part_pos.x) > render_distance ||
					abs(it->first.y - camera_part_pos.y) > render_distance) {
				if (it->second.node) {
					it->second.node->queue_free();
				}
				it = m_parts.erase(it);
				UtilityFunctions::print("Delete ", it->first);
			} else {
				++it;
			}
		}

		// Iterate over all parts that should be visible
		for (int z = -render_distance; z <= render_distance; ++z) {
			for (int x = -render_distance; x <= render_distance; ++x) {
				Vector2i part_pos = camera_part_pos + Vector2i(x, z);

				if (m_parts.find(part_pos) == m_parts.end()) {
					// This part is not loaded, so generate it
					Node3D *new_node = generate_part_at(part_pos);
					UtilityFunctions::print("Generate ", part_pos);
					if (new_node) {
						add_child(new_node);
						m_parts[part_pos] = { new_node };
					}
				}
			}
		}
	}
}

Node3D *MinecraftNode::generate_part_at(Vector2i part_pos) {
	// This function creates and returns a single terrain part.
	// It replaces the old logic that was in _process.
	String name = "Part_" + String::num_int64(part_pos.x) + "_" + String::num_int64(part_pos.y);

	// For now, we will just call the smooth mesh generator.
	// You can add more complex logic here later.
	generate_smooth_part_mesh(name, part_pos, false);

	Node3D *node = Object::cast_to<Node3D>(get_node_or_null(name));
	if (node) {
		Vector3 world_pos = Vector3(part_pos.x * part_size, 0, part_pos.y * part_size);
		node->set_position(world_pos);
	}
	return node;
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
