#include "mc_manager.h"
#include "mc.h"
#include "terrain.h"
#include "utils/raycast/mc_raycast.h"

#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void MCManager::_input(const Ref<InputEvent> &p_event) {
	// 2. Mouse Events
	Ref<InputEventMouseButton> mouse_event = p_event;
	if (mouse_event.is_null()) {
		return;
	}

	// Early return if not pressed, UNLESS we are in the middle of a drag and this is the release
	if (!mouse_event->is_pressed()) {
		if (!is_dragging) {
			return;
		}
	}

	MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
	if (!terrain_node) {
		return;
	}

	int button_index = mouse_event->get_button_index();
	if (button_index != MOUSE_BUTTON_LEFT && button_index != MOUSE_BUTTON_RIGHT) {
		return;
	}

	TypedArray<RID> exclude;
	if (is_dragging && drag_node) {
		// Find StaticBody3D child (there should only be one)
		TypedArray<Node> children = drag_node->get_children();
		for (int i = 0; i < children.size(); i++) {
			CollisionObject3D *co = Object::cast_to<CollisionObject3D>(children[i]);
			if (co) {
				exclude.push_back(co->get_rid());
			}
		}
	}

	uint32_t mask = 40; // Default: Layer 4 (Objects) | Layer 6 (Corners)

	MCRaycastHit hit = raycast_from_event(this, p_event, mask, 512, exclude);
	if (hit.is_hit) {
		Node3D *collider_node = Object::cast_to<Node3D>(hit.collider);
		if (!collider_node) {
			return;
		}

		Node3D *target_object_node = nullptr;
		Vector3i grid_pos;

		// Identify if we hit a placed object (Layer 4)
		CollisionObject3D *co = Object::cast_to<CollisionObject3D>(collider_node);
		if (co && co->get_collision_layer() & 8) {
			target_object_node = Object::cast_to<MeshInstance3D>(collider_node->get_parent());
		}

		if (target_object_node) {
			const PlacedObject *obj = terrain_node->get_placed_object(target_object_node);
			if (obj)
				grid_pos = obj->grid_pos;
		} else {
			// Hit a terrain corner
			Node *parent = collider_node->get_parent();
			Node3D *parent_node = Object::cast_to<Node3D>(parent);
			if (parent_node) {
				Vector3 hit_pos = parent_node->get_position();
				grid_pos = Vector3i(
						static_cast<int32_t>(Math::round(hit_pos.x)),
						static_cast<int32_t>(Math::round(hit_pos.y)),
						static_cast<int32_t>(Math::round(hit_pos.z)));
			}
		}

		if (button_index == MOUSE_BUTTON_LEFT) {
			if (interaction_mode == MODE_PLACE_OBJECT) {
				// 1. PLACE OBJECT MODE
				Vector3i place_pos = Vector3i((hit.position + hit.normal * 0.5f).floor());

				bool blocked = terrain_node->is_area_blocked_by_terrain(place_pos, current_placement_size);
				if (!blocked) {
					AABB volume = AABB(Vector3(place_pos), Vector3(current_placement_size));
					if (terrain_node->is_area_blocked_by_objects(volume)) {
						blocked = true;
					}
				}

				if (!blocked) {
					terrain_node->add_placed_object(place_pos, current_placement_size);
				} else {
					UtilityFunctions::print("MCManager: Cannot place object, area blocked.");
				}
			} else if (interaction_mode == MODE_DRAG_OBJECT) {
				// 2. DRAG OBJECT MODE (PICK UP)
				if (target_object_node && !is_dragging) {
					const PlacedObject *obj = terrain_node->get_placed_object(target_object_node);
					if (obj) {
						is_dragging = true;
						drag_node = Object::cast_to<MeshInstance3D>(target_object_node);
						drag_original_pos = obj->grid_pos;
						drag_size = obj->size;
						terrain_node->detach_placed_object(target_object_node);
					}
				}
			} else if (interaction_mode == MODE_TERRAIN) {
				// 3. TERRAIN MODE: Activate neighbor
				Vector3i target_pos = grid_pos + Vector3i(Math::round(hit.normal.x), Math::round(hit.normal.y), Math::round(hit.normal.z));
				terrain_node->modify_corner(target_pos, true);
			}
		} else if (button_index == MOUSE_BUTTON_RIGHT) {
			if (interaction_mode == MODE_PLACE_OBJECT || interaction_mode == MODE_DRAG_OBJECT) {
				if (target_object_node) {
					// SAFETY: If we are deleting the object we are currently dragging, cancel drag first
					if (is_dragging && drag_node == target_object_node) {
						is_dragging = false;
						drag_node = nullptr;
						UtilityFunctions::print("MCManager: Drag cancelled due to deletion.");
					}
					terrain_node->remove_placed_object(target_object_node);
				}
			} else if (interaction_mode == MODE_TERRAIN) {
				// Deactivate current corner
				terrain_node->modify_corner(grid_pos, false);
			}
		}
	}

	// Handle Drag Release (anywhere) - Now tied to LEFT MOUSE
	if (is_dragging && button_index == MOUSE_BUTTON_LEFT && !mouse_event->is_pressed()) {
		if (drag_valid) {
			// Finalize placement at new position
			terrain_node->add_placed_object(Vector3i((drag_node->get_position() - Vector3(drag_size) * 0.5f).round()), drag_size);
			drag_node->queue_free();
		} else {
			// Revert to original position
			terrain_node->add_placed_object(drag_original_pos, drag_size);
			drag_node->queue_free();
		}
		is_dragging = false;
		drag_node = nullptr;
	}
}

void MCManager::cancel_drag() {
	if (!is_dragging || !drag_node) {
		is_dragging = false; // Reset anyway
		drag_node = nullptr;
		return;
	}

	MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
	if (terrain_node) {
		// Revert to original position and add back to spatial
		terrain_node->add_placed_object(drag_original_pos, drag_size);
		UtilityFunctions::print("MCManager: Drag cancelled, object restored to original position.");
	}

	if (drag_node) {
		drag_node->queue_free();
	}

	is_dragging = false;
	drag_node = nullptr;
}

} // namespace godot
