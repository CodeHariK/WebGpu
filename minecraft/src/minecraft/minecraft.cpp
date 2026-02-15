#include "minecraft.h"

#include <cmath>
#include <cstdint>

#include "godot_cpp/classes/input.hpp"
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/transform3d.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include "godot_cpp/variant/vector2i.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>

#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/viewport.hpp>

using namespace godot;

MinecraftNode::MinecraftNode() {
	set_process_mode(Node::ProcessMode::PROCESS_MODE_INHERIT);
	m_last_camera_part_pos = Vector2i(INT32_MAX, INT32_MAX);
	m_last_camera_transform = Transform3D();
}

MinecraftNode::~MinecraftNode() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_process_mode(Node::ProcessMode::PROCESS_MODE_DISABLED);
	}
}

void MinecraftNode::_process(double delta) {
	if (!generate_on_ready) {
		return;
	}

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

	Transform3D current_camera_transform = camera->get_global_transform();

	if (m_last_camera_part_pos != camera_part_pos || m_last_camera_transform.basis != current_camera_transform.basis) {
		m_last_camera_part_pos = camera_part_pos;
		m_last_camera_transform = current_camera_transform;

		double current_system_time = Time::get_singleton()->get_unix_time_from_system();

		// Loop 1: Update visibility status of all loaded parts
		for (auto &pair : m_parts) { // Use reference to modify Part struct
			const Vector2i &part_pos = pair.first;
			Part &part_data = pair.second;

			bool should_be_visible = is_part_visible(part_pos, camera_part_pos);

			if (should_be_visible) {
				// part is within render distance and view angle
				part_data.visible = true;
				part_data.last_visible_time = 0.0; // Reset timestamp
			} else {
				// part is out of render distance or view angle
				if (part_data.visible) { // If it was previously visible
					part_data.visible = false;
					part_data.last_visible_time = current_system_time; // Store absolute time
				}
			}

			// --- Dynamic Collision Management ---
			// Define a collision distance (e.g., 3x3 grid around camera)
			bool should_have_collision = (abs(part_pos.x - camera_part_pos.x) <= 1 && abs(part_pos.y - camera_part_pos.y) <= 1);

			if (should_have_collision && !part_data.has_collision) {
				// Add collision to a nearby part that doesn't have it
				if (part_data.mesh) {
					part_data.mesh->create_trimesh_collision();
					part_data.has_collision = true;
				}
			} else if (!should_have_collision && part_data.has_collision) {
				// Remove collision from a distant part that has it
				if (part_data.mesh) {
					for (int i = 0; i < part_data.mesh->get_child_count(); ++i) {
						if (StaticBody3D *body = Object::cast_to<StaticBody3D>(part_data.mesh->get_child(i))) {
							body->queue_free();
							break; // Assume only one collision body per mesh
						}
					}
					part_data.has_collision = false;
				}
			}
		}

		// Loop 2: Iterate over all parts that should be visible and load new ones
		for (int z = -render_distance; z <= render_distance; ++z) {
			for (int x = -render_distance; x <= render_distance; ++x) {
				Vector2i part_pos = camera_part_pos + Vector2i(x, z);

				// Only try to load chunks that are actually visible
				if (!is_part_visible(part_pos, camera_part_pos)) {
					continue;
				}

				if (m_parts.find(part_pos) == m_parts.end()) {
					String name = "Part_" + String::num_int64(part_pos.x) + "_" + String::num_int64(part_pos.y);

					// Generate the mesh without collision. The dynamic system will add it if needed.
					// MeshInstance3D *new_mesh = generate_voxel_part_mesh(name, part_pos, false);
					MeshInstance3D *new_mesh = generate_smooth_part_mesh(name, part_pos, false);

					if (new_mesh) {
						add_child(new_mesh);
						m_parts[part_pos] = { new_mesh, true, false, 0.0 }; // Initialize Part struct, has_collision is always false initially.
					}
				}
			}
		}

		// Loop 3: Delete parts that have been invisible for too long

		for (auto it = m_parts.begin(); it != m_parts.end();) {
			Part &part_data = it->second;
			if (!part_data.visible && part_data.last_visible_time > 0.0 &&
					//
					(current_system_time - part_data.last_visible_time) > part_visibility_time_out_duration)
			//
			{
				// part is out of render distance, was visible before, and timeout passed
				if (part_data.mesh) {
					part_data.mesh->queue_free();
				}

				it = m_parts.erase(it);
			} else {
				++it;
			}
		}

		// Build the debug text
		String debug_text;
		debug_text += "Total Nodes: " + String::num_int64(get_tree()->get_node_count()) + "\n";
		debug_text += "Loaded Parts: " + String::num_int64(m_parts.size()) + "\n\n";
		debug_text += "Part Position | Visible | Timeout (s) | Collision \n";
		debug_text += "--------------------------------------\n";

		for (const auto &pair : m_parts) {
			const Vector2i &part_pos = pair.first;
			const Part &part_data = pair.second;

			String pos_str = "(" + String::num_int64(part_pos.x) + ", " + String::num_int64(part_pos.y) + ")";
			String vis_str = part_data.visible ? "Yes" : "No ";
			String col_str = part_data.has_collision ? "Yes" : "No ";
			String time_str;

			if (!part_data.visible && part_data.last_visible_time > 0.0) {
				double time_left = part_visibility_time_out_duration - (current_system_time - part_data.last_visible_time);
				time_str = String::num(time_left > 0 ? time_left : 0, 1);
			} else {
				time_str = " - ";
			}
			debug_text += pos_str.pad_zeros(2) + " | " + vis_str + " | " + time_str + " | " + col_str + "\n";
		}
		ui.terrain_debug_label->set_text(debug_text);
	}
}

bool MinecraftNode::is_part_visible(const Vector2i &part_pos, const Vector2i &camera_part_pos) const {
	// 1. Distance check (cheap)
	if (abs(part_pos.x - camera_part_pos.x) > render_distance ||
			abs(part_pos.y - camera_part_pos.y) > render_distance) {
		return false;
	}

	// 2. Angle check (more expensive)
	// Get camera's forward vector (on the XZ plane)
	Vector3 cam_forward = -camera->get_global_transform().basis.get_column(2);
	cam_forward.y = 0;
	// Avoid normalizing zero vector if camera looks straight up/down
	if (cam_forward.length_squared() < 0.001f) {
		return true; // Or handle as a special case, for now, just assume it's visible
	}
	cam_forward.normalize();

	// Get vector from camera to chunk center
	Vector3 part_world_pos = Vector3(part_pos.x * part_size + part_size / 2.0f, 0, part_pos.y * part_size + part_size / 2.0f);
	Vector3 cam_world_pos = camera->get_global_position();
	cam_world_pos.y = 0;

	Vector3 dir_to_part = (part_world_pos - cam_world_pos);

	// If the chunk is very close, we always consider it visible to avoid pop-in/pop-out when turning.
	if (dir_to_part.length_squared() < (part_size * 1.5f) * (part_size * 1.5f)) {
		return true;
	}

	dir_to_part.normalize();

	// A dot product > 0 means the angle is < 90 degrees. This gives a 180-degree field of view.
	return cam_forward.dot(dir_to_part) >= 0.0f;
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
