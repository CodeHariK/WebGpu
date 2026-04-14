#include "mc.h"
#include "mc_manager.h"
#include "mc_physics.h"
#include "terrain.h"
#include "utils/raycast/mc_raycast.h"
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <vector>

namespace godot {

void MCManager::_input(const Ref<InputEvent> &p_event) {
	// 1. Keyboard Events
	Ref<InputEventKey> key_event = p_event;
	if (key_event.is_valid() && key_event->is_pressed()) {
		if (interaction_mode == MODE_TERRAIN && is_locked) {
			Key keycode = key_event->get_keycode();
			bool handled = true;
			if (keycode == KEY_W)
				locked_grid_pos.z -= 1;
			else if (keycode == KEY_S)
				locked_grid_pos.z += 1;
			else if (keycode == KEY_A)
				locked_grid_pos.x -= 1;
			else if (keycode == KEY_D)
				locked_grid_pos.x += 1;
			else if (keycode == KEY_Q)
				locked_grid_pos.y -= 1;
			else if (keycode == KEY_E)
				locked_grid_pos.y += 1;
			else
				handled = false;

			if (handled) {
				update_ui();
				return;
			}
		}
	}

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

	if (button_index == MOUSE_BUTTON_RIGHT && mouse_event->is_pressed() && mouse_event->is_command_or_control_pressed()) {
		if (interaction_mode == MODE_TERRAIN) {
			is_locked = !is_locked;
			UtilityFunctions::print("MCManager: Hash Investigation Locked: ", is_locked);
			return;
		}
	}

	if (interaction_mode == MODE_TERRAIN && is_locked) {
		// Strictly for inspection while locked
		return;
	}

	if (button_index != MOUSE_BUTTON_LEFT && button_index != MOUSE_BUTTON_RIGHT) {
		return;
	}

	TypedArray<RID> exclude;
	if (is_dragging && !drag_group.empty()) {
		for (const auto &obj : drag_group) {
			if (!obj.node) continue;
			TypedArray<Node> children = obj.node->get_children();
			for (int i = 0; i < children.size(); i++) {
				CollisionObject3D *co = Object::cast_to<CollisionObject3D>(children[i]);
				if (co) {
					exclude.push_back(co->get_rid());
				}
			}
		}
	}

	uint32_t mask = LAYER_OBJECTS | LAYER_CORNERS;

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
		if (co && co->get_collision_layer() & LAYER_OBJECTS) {
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
				// 2. DRAG OBJECT MODE (PICK UP & SELECTION)
				if (!is_dragging) {
					MeshInstance3D *clicked_mi = target_object_node ? Object::cast_to<MeshInstance3D>(target_object_node) : nullptr;
					bool shift = mouse_event->is_shift_pressed();

					if (clicked_mi) {
						auto it = std::find(selected_nodes.begin(), selected_nodes.end(), clicked_mi);
						bool is_already_selected = (it != selected_nodes.end());

						if (shift) {
							// Toggle selection
							if (is_already_selected) {
								clicked_mi->set_material_override(nullptr);
								selected_nodes.erase(it);
							} else {
								selected_nodes.push_back(clicked_mi);
								clicked_mi->set_material_override(hover_mat_cyan);
							}
						} else {
							// Normal click
							if (is_already_selected) {
								// Start dragging existing selection
								is_dragging = true;
								drag_group.clear();
								const PlacedObject *pivot_obj = terrain_node->get_placed_object(clicked_mi);
								if (pivot_obj) {
									Vector3i pivot_pos = pivot_obj->grid_pos;
									for (MeshInstance3D *node : selected_nodes) {
										const PlacedObject *obj = terrain_node->get_placed_object(node);
										if (obj) {
											SelectedObject so;
											so.node = node;
											so.original_grid_pos = obj->grid_pos;
											so.size = obj->size;
											so.relative_offset = obj->grid_pos - pivot_pos;
											drag_group.push_back(so);
											terrain_node->detach_placed_object(node);
										}
									}
								}
							} else {
								// Clear old selection and select this one to drag
								for (MeshInstance3D *node : selected_nodes) {
									if (node) node->set_material_override(nullptr);
								}
								selected_nodes.clear();
								selected_nodes.push_back(clicked_mi);
								clicked_mi->set_material_override(hover_mat_cyan);

								// Immediately start drag
								is_dragging = true;
								drag_group.clear();
								const PlacedObject *obj = terrain_node->get_placed_object(clicked_mi);
								if (obj) {
									SelectedObject so;
									so.node = clicked_mi;
									so.original_grid_pos = obj->grid_pos;
									so.size = obj->size;
									so.relative_offset = Vector3i(0, 0, 0);
									drag_group.push_back(so);
									terrain_node->detach_placed_object(clicked_mi);
								}
							}
						}
					} else if (!shift) {
						// Clicked empty space without shift: Clear selection
						for (MeshInstance3D *node : selected_nodes) {
							if (node) node->set_material_override(nullptr);
						}
						selected_nodes.clear();
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
					MeshInstance3D *target_mi = Object::cast_to<MeshInstance3D>(target_object_node);
					// SAFETY: If we are deleting objects we are currently dragging/selecting, clean up
					if (is_dragging) {
						for (const auto &obj : drag_group) {
							if (obj.node == target_mi) {
								is_dragging = false;
								drag_group.clear();
								UtilityFunctions::print("MCManager: Multi-drag cancelled due to deletion.");
								break;
							}
						}
					}
					auto it = std::find(selected_nodes.begin(), selected_nodes.end(), target_mi);
					if (it != selected_nodes.end()) {
						selected_nodes.erase(it);
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
			// Finalize placement for all objects in group
			for (const auto &obj : drag_group) {
				Vector3 target_visual_pos = obj.node->get_position();
				Vector3i final_grid_pos = Vector3i((target_visual_pos - Vector3(obj.size) * 0.5f).round());
				terrain_node->add_placed_object(final_grid_pos, obj.size);
				obj.node->queue_free();
			}
		} else {
			// Revert all to original positions
			for (const auto &obj : drag_group) {
				terrain_node->add_placed_object(obj.original_grid_pos, obj.size);
				obj.node->queue_free();
			}
		}
		
		// Clear transient drag state, but KEEP persistent selection if you want
		// Most users expect selection to stay after drag.
		is_dragging = false;
		drag_group.clear();
		selected_nodes.clear(); // Actually, clearing selection for now to make highlights reset
	}
}

void MCManager::cancel_drag() {
	if (!is_dragging) {
		is_dragging = false;
		drag_group.clear();
		return;
	}

	MCTerrain *terrain_node = Object::cast_to<MCTerrain>(get_node_or_null(terrain.path));
	if (terrain_node) {
		for (const auto &obj : drag_group) {
			terrain_node->add_placed_object(obj.original_grid_pos, obj.size);
			if (obj.node) obj.node->queue_free();
		}
		UtilityFunctions::print("MCManager: Multi-drag cancelled, all objects restored.");
	}

	is_dragging = false;
	drag_group.clear();
	selected_nodes.clear();
}

} // namespace godot
