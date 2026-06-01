/**
 * @file mp_manager.cpp
 * @brief Manages mouse interaction, UI, raycasting, and placement logic for Marching Prism grids.
 * Module Path: src/terrain/marching_prism/mp_manager.cpp
 * Build Dependencies: godot-cpp, mp_manager.h, game_manager.h, mc_raycast.h
 */

#include "mp_manager.h"
#include "../../game_manager/game_constants.h"
#include "../../game_manager/game_manager.h"
#include "cui/cui.h"
#include "mp.h"
#include "mp_grid.h"
#include "utils/raycast/mc_raycast.h"
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void MPManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mp_node_path", "p_path"), &MPManager::set_mp_node_path);
	ClassDB::bind_method(D_METHOD("get_mp_node_path"), &MPManager::get_mp_node_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "mp_node_path"), "set_mp_node_path", "get_mp_node_path");

	ClassDB::bind_method(D_METHOD("set_terrain_path", "p_path"), &MPManager::set_terrain_path);
	ClassDB::bind_method(D_METHOD("get_terrain_path"), &MPManager::get_terrain_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "terrain_path"), "set_terrain_path", "get_terrain_path");

	ClassDB::bind_method(D_METHOD("initialize_all"), &MPManager::initialize_all);
	ClassDB::bind_method(D_METHOD("_on_toggle_ui"), &MPManager::_on_toggle_ui);
	ClassDB::bind_method(D_METHOD("_on_toggle_visual_corners"), &MPManager::_on_toggle_visual_corners);
	ClassDB::bind_method(D_METHOD("_on_debug_draw_mode_selected", "index"), &MPManager::_on_debug_draw_mode_selected);
	ClassDB::bind_method(D_METHOD("_on_gui_input", "p_event"), &MPManager::_on_gui_input);
	ClassDB::bind_method(D_METHOD("_on_toggle_placement_mode"), &MPManager::_on_toggle_placement_mode);
	ClassDB::bind_method(D_METHOD("_on_show_help"), &MPManager::_on_show_help);
	ClassDB::bind_method(D_METHOD("_on_save_terrain"), &MPManager::_on_save_terrain);
	ClassDB::bind_method(D_METHOD("_on_load_terrain"), &MPManager::_on_load_terrain);
}

MPManager::MPManager() {
	current_placement_size = Vector3i(2, 3, 4);
}

MPManager::~MPManager() {}

void MPManager::_ready() {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	UtilityFunctions::print("MPManager: Ready, triggering initialization sequence...");
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	call_deferred("initialize_all");
	set_process(true);
}

void MPManager::set_mp_node_path(const NodePath &p_path) { mp_node_path = p_path; }
NodePath MPManager::get_mp_node_path() const { return mp_node_path; }

void MPManager::set_terrain_path(const NodePath &p_path) { terrain.path = p_path; }
NodePath MPManager::get_terrain_path() const { return terrain.path; }

void MPManager::_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	perf.update_timer += p_delta;
	if (perf.update_timer >= 0.1) {
		perf.update_timer = 0.0;
		update_ui();
	}

	_update_hover_raycast();
}

void MPManager::initialize_all() {
	if (!is_inside_tree())
		return;

	MPNode *mp_node = Object::cast_to<MPNode>(get_node_or_null(mp_node_path));
	MPGrid *terrain_node = Object::cast_to<MPGrid>(get_node_or_null(terrain.path));

	if (!mp_node || !terrain_node)
		return;

	mp_node->initialize_library();

	terrain_node->set_mp_node(mp_node);
	terrain_node->initialize_grid(terrain_node->get_grid_size().x, terrain_node->get_grid_size().y, terrain_node->get_grid_size().z,
								  terrain_node->get_chunk_size().x, terrain_node->get_chunk_size().y, terrain_node->get_chunk_size().z);

	ui.manager = CUI::create_on_new_layer(this);
	if (ui.manager)
		setup_ui();
	update_ui();

	_initialize_previews();

	GameManager *gm = GameManager::get_singleton();
	if (gm)
		gm->register_mp_manager(this);
}

void MPManager::_on_load_terrain() { load_terrain("user://prism_terrain.mpt"); }
void MPManager::_on_save_terrain() { save_terrain("user://prism_terrain.mpt"); }

void MPManager::save_terrain(const String &p_path) {
	MPGrid *terrain_node = Object::cast_to<MPGrid>(get_node_or_null(terrain.path));
	if (terrain_node)
		terrain_node->save_grid(p_path);
}

void MPManager::load_terrain(const String &p_path) {
	MPGrid *terrain_node = Object::cast_to<MPGrid>(get_node_or_null(terrain.path));
	if (terrain_node)
		terrain_node->load_grid(p_path);
}

uint8_t MPManager::_get_cell_hash(const Vector3i &p_grid_pos) {
	MPGrid *terrain_node = Object::cast_to<MPGrid>(get_node_or_null(terrain.path));
	if (!terrain_node)
		return 0;

	Vector3i chunk_sz = terrain_node->get_chunk_size();
	Vector3i grid_sz = terrain_node->get_grid_size();

	int cx = p_grid_pos.x; // Global Cell X
	int y = p_grid_pos.y;
	int z = p_grid_pos.z;

	int chunk_x = cx / (chunk_sz.x * 2);
	int chunk_y = y / chunk_sz.y;
	int chunk_z = z / chunk_sz.z;

	if (chunk_x < 0 || chunk_x >= grid_sz.x || chunk_y < 0 || chunk_y >= grid_sz.y || chunk_z < 0 || chunk_z >= grid_sz.z) {
		return 0;
	}

	int local_cx = cx % (chunk_sz.x * 2);
	int local_y = y % chunk_sz.y;

	int vx = local_cx / 2;
	int v0_x, v0_z, v1_x, v1_z, v2_x, v2_z;

	// Bind Parity entirely to Global Coordinates to avoid odd-chunk-size desyncs
	bool points_down = ((cx + z) % 2 == 0);

	if (z % 2 == 0) {
		if (points_down) {
			v1_x = vx;
			v1_z = z;
			v0_x = vx + 1;
			v0_z = z;
			v2_x = vx;
			v2_z = z + 1;
		} else {
			v0_x = vx;
			v0_z = z + 1;
			v1_x = vx + 1;
			v1_z = z + 1;
			v2_x = vx + 1;
			v2_z = z;
		}
	} else {
		if (points_down) {
			v1_x = vx;
			v1_z = z;
			v0_x = vx + 1;
			v0_z = z;
			v2_x = vx + 1;
			v2_z = z + 1;
		} else {
			v0_x = vx;
			v0_z = z + 1;
			v1_x = vx + 1;
			v1_z = z + 1;
			v2_x = vx;
			v2_z = z;
		}
	}

	int global_v0_x = chunk_x * chunk_sz.x + v0_x;
	int global_v0_z = chunk_z * chunk_sz.z + v0_z;
	int global_v1_x = chunk_x * chunk_sz.x + v1_x;
	int global_v1_z = chunk_z * chunk_sz.z + v1_z;
	int global_v2_x = chunk_x * chunk_sz.x + v2_x;
	int global_v2_z = chunk_z * chunk_sz.z + v2_z;
	int global_y = chunk_y * chunk_sz.y + local_y;

	uint8_t config_idx = 0;
	if (terrain_node->is_corner_active(Vector3i(global_v0_x, global_y, global_v0_z)))
		config_idx |= (1 << 0);
	if (terrain_node->is_corner_active(Vector3i(global_v1_x, global_y, global_v1_z)))
		config_idx |= (1 << 1);
	if (terrain_node->is_corner_active(Vector3i(global_v2_x, global_y, global_v2_z)))
		config_idx |= (1 << 2);
	if (terrain_node->is_corner_active(Vector3i(global_v0_x, global_y + 1, global_v0_z)))
		config_idx |= (1 << 3);
	if (terrain_node->is_corner_active(Vector3i(global_v1_x, global_y + 1, global_v1_z)))
		config_idx |= (1 << 4);
	if (terrain_node->is_corner_active(Vector3i(global_v2_x, global_y + 1, global_v2_z)))
		config_idx |= (1 << 5);

	return config_idx;
}

void MPManager::cancel_drag() {
	is_dragging = false;
	drag_group.clear();
}

void MPManager::_initialize_previews() {
	hover_mat_yellow.instantiate();
	hover_mat_yellow->set_albedo(Color(1, 1, 0, 0.4));
	hover_mat_yellow->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	hover_mat_yellow->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	hover_mat_red.instantiate();
	hover_mat_red->set_albedo(Color(1, 0, 0, 0.6));
	hover_mat_red->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	hover_mat_red->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	hover_mat_cyan.instantiate();
	hover_mat_cyan->set_albedo(Color(0, 1, 1, 0.6));
	hover_mat_cyan->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	hover_mat_cyan->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
}

void MPManager::_update_hover_raycast() {
	MPGrid *terrain_node = Object::cast_to<MPGrid>(get_node_or_null(terrain.path));
	if (terrain_node)
		terrain_node->hide_hover_preview();

	Viewport *viewport = get_viewport();
	if (!viewport)
		return;

	bool is_ctrl = Input::get_singleton()->is_key_pressed(KEY_CTRL);
	if (terrain_node) {
		terrain_node->set_corner_collision_enabled(!is_ctrl);
		terrain_node->set_cell_collision_enabled(is_ctrl);
	}

	uint32_t mask = is_ctrl ? toLayer(LAYER_CELLS) : toLayer(LAYER_CORNERS);

	TypedArray<RID> exclude;
	MCRaycastHit hit = raycast_from_mouse(this, viewport->get_mouse_position(), mask, 1000.0f, exclude);

	if (hit.is_hit) {
		Node3D *collider_node = Object::cast_to<Node3D>(hit.collider);
		if (collider_node) {
			Camera3D *camera = viewport->get_camera_3d();

			if (is_ctrl && collider_node->has_meta("is_cell")) {
				// Hit cell center (LAYER_CELLS)
				int cx = collider_node->get_meta("cell_x");
				int cy = collider_node->get_meta("cell_y");
				int cz = collider_node->get_meta("cell_z");
				locked_grid_pos = Vector3i(cx, cy, cz);
				is_hovering_cell = true;
				update_ui();

				// Option: Light up center of cell with debug square
				if (terrain_node)
					terrain_node->update_hover_preview(collider_node->get_global_position(), hit.normal, camera, true);

			} else if (!is_ctrl) {
				// Hit corner (LAYER_CORNERS)
				Node *parent = collider_node->get_parent();
				Node3D *parent_node = Object::cast_to<Node3D>(parent);
				if (parent_node) {
					Vector3 hit_pos = parent_node->get_position();
					int gy = static_cast<int32_t>(Math::round(hit_pos.y));
					int gz = static_cast<int32_t>(Math::round(hit_pos.z / 0.866025f));
					float stagger = (gz % 2 != 0) ? 0.5f : 0.0f;
					int gx = static_cast<int32_t>(Math::round(hit_pos.x - stagger));

					locked_grid_pos = Vector3i(gx, gy, gz);
					is_hovering_cell = false;
					update_ui();

					if (terrain_node)
						terrain_node->update_hover_preview(hit_pos, hit.normal, camera, false);
				}
			}
		}
	}
}

void MPManager::_on_toggle_visual_corners() {
	MPGrid *terrain_node = Object::cast_to<MPGrid>(get_node_or_null(terrain.path));
	if (terrain_node) {
		bool current = terrain_node->is_debug_corners_visible();
		terrain_node->set_debug_corners_visible(!current);
		UtilityFunctions::print("MPManager: Visual Corners toggled to ", !current);
	}
}

void MPManager::_on_debug_draw_mode_selected(int p_index) {
	MPGrid *terrain_node = Object::cast_to<MPGrid>(get_node_or_null(terrain.path));
	if (terrain_node) {
		terrain_node->set_debug_draw_mode((MPGrid::DebugDrawMode)p_index);
		UtilityFunctions::print("MPManager: Debug Draw Mode selected: ", p_index);
	}
}

void MPManager::_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mouse_event = p_event;
	if (mouse_event.is_null() || !mouse_event->is_pressed())
		return;

	MPGrid *terrain_node = Object::cast_to<MPGrid>(get_node_or_null(terrain.path));
	if (!terrain_node)
		return;

	int button_index = mouse_event->get_button_index();
	if (button_index != MOUSE_BUTTON_LEFT && button_index != MOUSE_BUTTON_RIGHT)
		return;

	bool is_ctrl = Input::get_singleton()->is_key_pressed(KEY_CTRL);
	uint32_t mask = is_ctrl ? toLayer(LAYER_CELLS) : toLayer(LAYER_CORNERS);

	TypedArray<RID> exclude;
	MCRaycastHit hit = raycast_from_event(this, p_event, mask, 512, exclude);
	if (hit.is_hit) {
		Node3D *collider_node = Object::cast_to<Node3D>(hit.collider);
		if (collider_node) {
			if (is_ctrl && collider_node->has_meta("is_cell")) {
				int cx = collider_node->get_meta("cell_x");
				int cy = collider_node->get_meta("cell_y");
				int cz = collider_node->get_meta("cell_z");
				locked_grid_pos = Vector3i(cx, cy, cz);
				is_hovering_cell = true;
				update_ui();
			} else if (!is_ctrl) {
				Node *parent = collider_node->get_parent();
				Node3D *parent_node = Object::cast_to<Node3D>(parent);
				if (parent_node) {
					Vector3 hit_pos = parent_node->get_position();
					int gy = static_cast<int32_t>(Math::round(hit_pos.y));
					int gz = static_cast<int32_t>(Math::round(hit_pos.z / 0.866025f));
					float stagger = (gz % 2 != 0) ? 0.5f : 0.0f;
					int gx = static_cast<int32_t>(Math::round(hit_pos.x - stagger));

					Vector3i grid_pos = Vector3i(gx, gy, gz);

					if (button_index == MOUSE_BUTTON_LEFT) {
						Vector3i normal_dir = _get_staggered_normal_dir(hit.normal, grid_pos);
						terrain_node->modify_corner(grid_pos + normal_dir, true);
					} else if (button_index == MOUSE_BUTTON_RIGHT) {
						terrain_node->modify_corner(grid_pos, false);
					}
				}
			}
		}
	}
}

Vector3i MPManager::_get_staggered_normal_dir(const Vector3 &p_hit_normal, const Vector3i &p_grid_pos) const {
	Vector3 directions[8] = {
		Vector3(1.0f, 0.0f, 0.0f),
		Vector3(-1.0f, 0.0f, 0.0f),
		Vector3(0.5f, 0.0f, 0.866025f),
		Vector3(-0.5f, 0.0f, 0.866025f),
		Vector3(0.5f, 0.0f, -0.866025f),
		Vector3(-0.5f, 0.0f, -0.866025f),
		Vector3(0.0f, 1.0f, 0.0f),
		Vector3(0.0f, -1.0f, 0.0f)
	};

	Vector3 closest_dir = directions[0];
	float max_dot = -2.0f;
	for (int i = 0; i < 8; ++i) {
		float dot = p_hit_normal.dot(directions[i]);
		if (dot > max_dot) {
			max_dot = dot;
			closest_dir = directions[i];
		}
	}

	Vector3i normal_dir(0, 0, 0);
	if (closest_dir.is_equal_approx(Vector3(1.0f, 0.0f, 0.0f))) {
		normal_dir = Vector3i(1, 0, 0);
	} else if (closest_dir.is_equal_approx(Vector3(-1.0f, 0.0f, 0.0f))) {
		normal_dir = Vector3i(-1, 0, 0);
	} else if (closest_dir.is_equal_approx(Vector3(0.0f, 1.0f, 0.0f))) {
		normal_dir = Vector3i(0, 1, 0);
	} else if (closest_dir.is_equal_approx(Vector3(0.0f, -1.0f, 0.0f))) {
		normal_dir = Vector3i(0, -1, 0);
	} else {
		int gz = p_grid_pos.z;
		bool is_even = (gz % 2 == 0);
		if (closest_dir.is_equal_approx(Vector3(0.5f, 0.0f, 0.866025f))) {
			normal_dir = is_even ? Vector3i(0, 0, 1) : Vector3i(1, 0, 1);
		} else if (closest_dir.is_equal_approx(Vector3(-0.5f, 0.0f, 0.866025f))) {
			normal_dir = is_even ? Vector3i(-1, 0, 1) : Vector3i(0, 0, 1);
		} else if (closest_dir.is_equal_approx(Vector3(0.5f, 0.0f, -0.866025f))) {
			normal_dir = is_even ? Vector3i(0, 0, -1) : Vector3i(1, 0, -1);
		} else if (closest_dir.is_equal_approx(Vector3(-0.5f, 0.0f, -0.866025f))) {
			normal_dir = is_even ? Vector3i(-1, 0, -1) : Vector3i(0, 0, -1);
		}
	}
	return normal_dir;
}

} // namespace godot
