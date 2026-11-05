// MCManager: inspect mode (the grid side is grid/mc_grid_inspect.cpp).
//
// With Inspect on (Stats panel → Diagnostics) and the Terrain or Water tool, the hover picks cells
// (like the Ctrl / Cmd / Shift cell hover) and the mouse no longer edits the terrain or the water:
//   left click a cell     it is marked (orange wire cube) and shown in the inspect panel
//   right click           clear
// The inspect panel (bottom right) is a SubViewport with its own small world: the cell's tile with
// its wireframe, its cube and the 8 corner dots numbered by hash bit (orange active, black
// inactive), lit and slowly turning; drag in it to orbit, scroll to zoom. The text under it says
// what the cell is (MCGrid::get_cell_info). The terrain itself never moves or changes.
#include "marching_cubes/editor/mc_manager.h"
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/util/mc_physics.h"
#include "utils/raycast/mc_raycast.h"
#include "cui/cui.h"
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/sub_viewport_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

namespace {
const Vector2i VIEW_SIZE(340, 300);
const float SPIN_SPEED = 0.35f; // radians a second while nobody is orbiting
} // namespace

// Inspect on / off.
void MCManager::_on_toggle_inspect() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid) {
		return;
	}
	grid->set_inspect_mode(!grid->is_inspect_mode());
	grid->set_debug_seams(grid->is_inspect_mode()); // the tile seams while inspecting
	if (!grid->is_inspect_mode() && inspect.panel) {
		inspect.panel->hide();
	}
	UtilityFunctions::print("MCManager: inspect mode ", grid->is_inspect_mode() ? "on" : "off");
}

void MCManager::inspect_at_cell(const Vector3i &p_cell) {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (!grid) {
		return;
	}
	grid->set_inspect_mode(true);
	grid->inspect_cell(p_cell);
	_show_inspect_cell(grid, p_cell);
}

// Is inspect mode taking the mouse (inspect on, Terrain or Water tool)?
bool MCManager::_inspect_active(MCGrid *p_grid) const {
	return p_grid && p_grid->is_inspect_mode() && (interaction_mode == MODE_TERRAIN || interaction_mode == MODE_WATER);
}

// Build the panel: a frame in the bottom-right corner with the 3D view and the text under it.
void MCManager::_setup_inspect_panel() {
	if (inspect.panel || !ui.manager) {
		return;
	}
	PanelContainer *panel = memnew(PanelContainer);
	panel->set_name("InspectPanel");
	Ref<StyleBoxFlat> frame;
	frame.instantiate();
	frame->set_bg_color(Color(0.1f, 0.11f, 0.13f, 0.92f));
	frame->set_corner_radius_all(10);
	frame->set_content_margin_all(8);
	panel->add_theme_stylebox_override("panel", frame);
	panel->set_anchors_and_offsets_preset(Control::PRESET_BOTTOM_RIGHT);
	panel->set_offset(SIDE_LEFT, -VIEW_SIZE.x - 32);
	panel->set_offset(SIDE_TOP, -VIEW_SIZE.y - 150);
	panel->set_offset(SIDE_RIGHT, -16);
	panel->set_offset(SIDE_BOTTOM, -16);
	ui.manager->add_child(panel);

	VBoxContainer *column = memnew(VBoxContainer);
	panel->add_child(column);

	// the 3D view: its own world, so only the inspected cell is in it
	SubViewportContainer *container = memnew(SubViewportContainer);
	container->set_stretch(true);
	container->set_custom_minimum_size(Vector2(VIEW_SIZE));
	container->connect("gui_input", Callable(this, "_on_inspect_view_input"));
	column->add_child(container);
	SubViewport *viewport = memnew(SubViewport);
	viewport->set_use_own_world_3d(true);
	viewport->set_size(VIEW_SIZE);
	viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);
	viewport->set_msaa_3d(Viewport::MSAA_4X);
	container->add_child(viewport);

	Ref<Environment> environment;
	environment.instantiate();
	environment->set_background(Environment::BG_COLOR);
	environment->set_bg_color(Color(0.17f, 0.18f, 0.21f));
	environment->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
	environment->set_ambient_light_color(Color(1, 1, 1));
	environment->set_ambient_light_energy(0.55f);
	WorldEnvironment *world = memnew(WorldEnvironment);
	world->set_environment(environment);
	viewport->add_child(world);
	DirectionalLight3D *sun = memnew(DirectionalLight3D);
	sun->set_rotation_degrees(Vector3(-50, 35, 0));
	viewport->add_child(sun);

	inspect.pivot = memnew(Node3D); // turns the camera round the cell
	viewport->add_child(inspect.pivot);
	inspect.camera = memnew(Camera3D);
	inspect.camera->set_fov(40);
	inspect.pivot->add_child(inspect.camera);
	inspect.camera->set_current(true);

	inspect.info = memnew(Label);
	inspect.info->add_theme_font_size_override("font_size", 14);
	inspect.info->set_custom_minimum_size(Vector2(VIEW_SIZE.x, 0));
	column->add_child(inspect.info);
	Label *hint = memnew(Label);
	hint->set_text(String::utf8("drag: orbit  ·  scroll: zoom  ·  right click: clear"));
	hint->add_theme_font_size_override("font_size", 11);
	hint->set_modulate(Color(1, 1, 1, 0.6f));
	column->add_child(hint);

	inspect.panel = panel;
	_place_inspect_camera();
	panel->hide();
}

// Put the camera where yaw / pitch / distance say, looking at the cell centre (the origin).
void MCManager::_place_inspect_camera() {
	if (!inspect.pivot) {
		return;
	}
	inspect.pivot->set_rotation(Vector3(inspect.pitch, inspect.yaw, 0));
	inspect.camera->set_position(Vector3(0, 0, inspect.distance));
}

// Show a cell in the panel (a fresh preview from the grid) with its text.
void MCManager::_show_inspect_cell(
		MCGrid *p_grid,
		const Vector3i &p_cell
) {
	_setup_inspect_panel();
	if (!inspect.panel) {
		return;
	}
	if (inspect.preview) {
		inspect.preview->queue_free();
	}
	inspect.preview = p_grid->make_cell_preview(p_cell);
	inspect.pivot->get_parent()->add_child(inspect.preview);
	inspect.info->set_text(p_grid->get_cell_info(p_cell));
	inspect.panel->show();
}

// Drag in the view to orbit, scroll to zoom.
void MCManager::_on_inspect_view_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid() && (motion->get_button_mask().has_flag(MOUSE_BUTTON_MASK_LEFT))) {
		inspect.spinning = false; // once orbited by hand it stays where it was left
		inspect.yaw -= motion->get_relative().x * 0.01f;
		inspect.pitch = Math::clamp(inspect.pitch - motion->get_relative().y * 0.01f, -1.45f, 1.45f);
		_place_inspect_camera();
		return;
	}
	Ref<InputEventMouseButton> button = p_event;
	if (button.is_valid() && button->is_pressed()) {
		if (button->get_button_index() == MOUSE_BUTTON_WHEEL_UP) {
			inspect.distance = Math::max(inspect.distance * 0.9f, 1.2f);
		} else if (button->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN) {
			inspect.distance = Math::min(inspect.distance * 1.1f, 8.0f);
		}
		_place_inspect_camera();
	}
}

// Every frame while the panel is up: turn slowly until someone orbits by hand.
void MCManager::_update_inspect_view(double p_delta) {
	if (!inspect.panel || !inspect.panel->is_visible() || !inspect.spinning) {
		return;
	}
	inspect.yaw += static_cast<float>(p_delta) * SPIN_SPEED;
	_place_inspect_camera();
}

// Clicks in inspect mode: left picks the cell under the mouse, right clears. True when used.
bool MCManager::_inspect_input(
		MCGrid *p_grid,
		const Ref<InputEvent> &p_event
) {
	if (!_inspect_active(p_grid)) {
		return false;
	}
	Ref<InputEventMouseButton> button = p_event;
	if (button.is_null() || !button->is_pressed()) {
		return false;
	}
	MouseButton index = button->get_button_index();
	if (index != MOUSE_BUTTON_LEFT && index != MOUSE_BUTTON_RIGHT) {
		return false;
	}
	if (index == MOUSE_BUTTON_RIGHT) {
		p_grid->clear_inspect();
		if (inspect.panel) {
			inspect.panel->hide();
		}
		return true;
	}
	MCRaycastHit hit = raycast_from_event(this, p_event, toLayer(LAYER_TERRAIN), 512);
	if (!hit.is_hit) {
		return true; // in inspect mode a click never edits the terrain
	}
	// the cell just inside the surface that was hit (the same one the hover shows)
	Vector3 local = p_grid->to_local(hit.position);
	Vector3 normal = (p_grid->get_global_transform().basis.inverse().xform(hit.normal)).normalized();
	Vector3i cell = Vector3i((local - normal * 0.05f).floor());
	p_grid->inspect_cell(cell);
	_show_inspect_cell(p_grid, cell);
	_record_inspect(p_grid, cell, "inspected"); // camera, cell, map, screenshot (mc_manager_record.cpp)
	return true;
}

} // namespace godot
