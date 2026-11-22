/**
 * @file city_minimap.cpp
 * @brief CityMinimap: the picture taken once from above, then each frame the part round the player
 * drawn in a corner, or the open map (any part of the picture, panned and zoomed) in the middle of
 * the screen; the player an arrow.
 */
#include "city_minimap.h"
#include "../game_manager/game_manager.h"
#include "city_map.h"
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_magnify_gesture.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

namespace {

constexpr float MARGIN = 16.0f; // pixels from the screen's edge
constexpr float CAMERA_HEIGHT = 1500.0f; // metres over the city
constexpr int WAIT_FRAMES = 3; // frames after the city is built before taking the picture
constexpr float MAX_ZOOM = 12.0f; // the open map zooms in this far at most
constexpr float WHEEL_STEP = 1.2f; // zoom per wheel notch
constexpr float CLOSE_SIZE = 28.0f; // pixels: the × box
const Color FRAME_COLOUR = Color(0.08f, 0.07f, 0.07f, 0.9f);
const Color ARROW_COLOUR = Color(1.0f, 0.25f, 0.2f, 1.0f);
const Color CLOSE_COLOUR = Color(1.0f, 1.0f, 1.0f, 0.9f);

} // namespace

void CityMinimap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_city_map_path", "path"), &CityMinimap::set_city_map_path);
	ClassDB::bind_method(D_METHOD("get_city_map_path"), &CityMinimap::get_city_map_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "city_map_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "CityMap"), "set_city_map_path", "get_city_map_path");
	ClassDB::bind_method(D_METHOD("set_corner_size", "size"), &CityMinimap::set_corner_size);
	ClassDB::bind_method(D_METHOD("get_corner_size"), &CityMinimap::get_corner_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "corner_size"), "set_corner_size", "get_corner_size");
	ClassDB::bind_method(D_METHOD("set_corner_metres", "metres"), &CityMinimap::set_corner_metres);
	ClassDB::bind_method(D_METHOD("get_corner_metres"), &CityMinimap::get_corner_metres);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "corner_metres"), "set_corner_metres", "get_corner_metres");
	ClassDB::bind_method(D_METHOD("set_full_map", "full"), &CityMinimap::set_full_map);
	ClassDB::bind_method(D_METHOD("get_full_map"), &CityMinimap::get_full_map);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "full_map"), "set_full_map", "get_full_map");
}

CityMap *CityMinimap::_city_map() const {
	return city_map_path.is_empty() ? nullptr : Object::cast_to<CityMap>(get_node_or_null(city_map_path));
}

const Node3D *CityMinimap::_target() const {
	GameManager *game = GameManager::get_singleton();
	return game ? Object::cast_to<Node3D>(game->get_active_target()) : nullptr;
}

void CityMinimap::_ready() {
	set_mouse_filter(MOUSE_FILTER_STOP);
	viewport = memnew(SubViewport);
	viewport->set_name("MapPicture");
	viewport->set_size(Vector2i(resolution, resolution));
	viewport->set_update_mode(SubViewport::UPDATE_DISABLED);
	add_child(viewport);
	camera = memnew(Camera3D);
	camera->set_projection(Camera3D::PROJECTION_ORTHOGONAL);
	camera->set_near(1.0f);
	camera->set_far(CAMERA_HEIGHT * 2.0f);
	viewport->add_child(camera);
	view_middle = Vector2(resolution, resolution) * 0.5f;
	set_process(true);
	set_process_unhandled_input(true);
}

void CityMinimap::_take_picture() {
	CityMap *map = _city_map();
	if (!map || !map->get_city() || map->get_map_size() <= 0.0f) {
		return;
	}
	map_size = map->get_map_size();
	centre = map->get_city()->get_global_position(); // the map's middle
	camera->set_size(map_size);
	camera->set_global_transform(Transform3D(Basis::looking_at(Vector3(0, -1, 0), Vector3(0, 0, -1)), centre + Vector3(0, CAMERA_HEIGHT, 0)));
	camera->make_current();
	viewport->set_update_mode(SubViewport::UPDATE_ONCE);
	taken = true;
}

// The corner map in the top left; the open map a square in the middle of the screen.
void CityMinimap::_place() {
	const Vector2 screen = get_viewport_rect().size;
	if (full_map) {
		const float side = MIN(screen.x, screen.y) * 0.9f;
		set_position((screen - Vector2(side, side)) * 0.5f);
		set_size(Vector2(side, side));
	} else {
		set_position(Vector2(MARGIN, MARGIN));
		set_size(Vector2(corner_size, corner_size));
	}
}

void CityMinimap::_process(double p_delta) {
	if (!taken && ++frames_waited >= WAIT_FRAMES) {
		_take_picture();
	}
	_place();
	queue_redraw();
}

Vector2 CityMinimap::_to_picture(const Vector3 &p_world) const {
	return Vector2((p_world.x - centre.x) / map_size + 0.5f, (p_world.z - centre.z) / map_size + 0.5f) * float(resolution);
}

Rect2 CityMinimap::_shown() const {
	const float across = float(resolution) / view_zoom;
	return Rect2(view_middle - Vector2(across, across) * 0.5f, Vector2(across, across));
}

Rect2 CityMinimap::_close_box() const {
	return Rect2(Vector2(get_size().x - CLOSE_SIZE - 6.0f, 6.0f), Vector2(CLOSE_SIZE, CLOSE_SIZE));
}

// Zooms the open map by p_factor, keeping the picture point under p_at (pixels in the control) still.
void CityMinimap::_zoom_at(
		const Vector2 &p_at,
		float p_factor
) {
	const Rect2 before = _shown();
	const Vector2 under = before.position + p_at / get_size() * before.size.x;
	view_zoom = CLAMP(view_zoom * p_factor, 1.0f, MAX_ZOOM);
	const float across = float(resolution) / view_zoom;
	view_middle = under - (p_at / get_size() - Vector2(0.5f, 0.5f)) * across;
	_pan_by(Vector2()); // keep it on the picture
}

// Moves the open map by p_screen_delta (pixels: the picture follows the pointer), never off the picture.
void CityMinimap::_pan_by(const Vector2 &p_screen_delta) {
	const float across = float(resolution) / view_zoom;
	view_middle -= p_screen_delta / get_size().x * across;
	const float half = across * 0.5f;
	view_middle.x = CLAMP(view_middle.x, half, float(resolution) - half);
	view_middle.y = CLAMP(view_middle.y, half, float(resolution) - half);
}

void CityMinimap::_draw_arrow(
		const Vector2 &p_at,
		const Node3D *p_target,
		float p_size
) {
	const Vector3 forward = -p_target->get_global_basis().get_column(2); // the way it faces
	const float angle = Math::atan2(forward.x, -forward.z); // 0: up the map (−z)
	PackedVector2Array points;
	points.push_back(p_at + Vector2(0, -p_size).rotated(angle));
	points.push_back(p_at + Vector2(p_size * 0.6f, p_size * 0.7f).rotated(angle));
	points.push_back(p_at + Vector2(0, p_size * 0.35f).rotated(angle));
	points.push_back(p_at + Vector2(-p_size * 0.6f, p_size * 0.7f).rotated(angle));
	draw_colored_polygon(points, ARROW_COLOUR);
}

void CityMinimap::_draw_open() {
	const Vector2 size = get_size();
	const Rect2 shown = _shown();
	draw_texture_rect_region(viewport->get_texture(), Rect2(Vector2(), size), shown);
	const Node3D *target = _target();
	if (target) {
		const Vector2 at = (_to_picture(target->get_global_position()) - shown.position) / shown.size.x * size.x;
		if (Rect2(Vector2(), size).has_point(at)) {
			_draw_arrow(at, target, 11.0f);
		}
	}
	const Rect2 box = _close_box();
	draw_rect(box, FRAME_COLOUR);
	draw_line(box.position + Vector2(7, 7), box.get_end() - Vector2(7, 7), CLOSE_COLOUR, 2.0f);
	draw_line(Vector2(box.get_end().x - 7, box.position.y + 7), Vector2(box.position.x + 7, box.get_end().y - 7), CLOSE_COLOUR, 2.0f);
}

void CityMinimap::_draw_corner() {
	const Vector2 size = get_size();
	const Node3D *target = _target();
	const Vector2 middle = target ? _to_picture(target->get_global_position()) : Vector2(resolution, resolution) * 0.5f;
	const float across = corner_metres / map_size * float(resolution); // picture pixels shown across
	draw_texture_rect_region(viewport->get_texture(), Rect2(Vector2(), size), Rect2(middle - Vector2(across, across) * 0.5f, Vector2(across, across)));
	if (target) {
		_draw_arrow(size * 0.5f, target, 9.0f);
	}
}

void CityMinimap::_draw() {
	draw_rect(Rect2(Vector2(-3, -3), get_size() + Vector2(6, 6)), FRAME_COLOUR);
	if (!taken || map_size <= 0.0f) {
		return;
	}
	if (full_map) {
		_draw_open();
	} else {
		_draw_corner();
	}
}

void CityMinimap::_gui_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseButton> button = p_event;
	const Ref<InputEventScreenTouch> touch = p_event;
	const bool pressed = (button.is_valid() && button->is_pressed() && button->get_button_index() == MOUSE_BUTTON_LEFT) || (touch.is_valid() && touch->is_pressed());
	const bool released = (button.is_valid() && !button->is_pressed() && button->get_button_index() == MOUSE_BUTTON_LEFT) || (touch.is_valid() && !touch->is_pressed());
	if (!full_map) { // the corner map: a tap opens the whole map
		if (pressed) {
			set_full_map(true);
			accept_event();
		}
		return;
	}
	if (pressed) {
		const Vector2 at = button.is_valid() ? button->get_position() : touch->get_position();
		if (_close_box().has_point(at)) {
			set_full_map(false);
		} else {
			dragging = true;
		}
		accept_event();
		return;
	}
	if (released) {
		dragging = false;
		accept_event();
		return;
	}
	if (button.is_valid() && button->is_pressed() && (button->get_button_index() == MOUSE_BUTTON_WHEEL_UP || button->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN)) {
		_zoom_at(button->get_position(), button->get_button_index() == MOUSE_BUTTON_WHEEL_UP ? WHEEL_STEP : 1.0f / WHEEL_STEP);
		accept_event();
		return;
	}
	const Ref<InputEventMouseMotion> motion = p_event;
	const Ref<InputEventScreenDrag> drag = p_event;
	if (dragging && motion.is_valid()) {
		_pan_by(motion->get_relative());
		accept_event();
	} else if (drag.is_valid()) {
		_pan_by(drag->get_relative());
		accept_event();
	}
	const Ref<InputEventMagnifyGesture> pinch = p_event;
	if (pinch.is_valid()) {
		_zoom_at(pinch->get_position(), pinch->get_factor());
		accept_event();
	}
}

void CityMinimap::_unhandled_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventKey> key = p_event;
	if (key.is_null() || !key->is_pressed() || key->is_echo()) {
		return;
	}
	if (key->get_physical_keycode() == KEY_M || (full_map && key->get_physical_keycode() == KEY_ESCAPE)) {
		set_full_map(key->get_physical_keycode() == KEY_M ? !full_map : false);
		get_viewport()->set_input_as_handled();
	}
}

void CityMinimap::set_city_map_path(const NodePath &p_path) {
	city_map_path = p_path;
}

NodePath CityMinimap::get_city_map_path() const {
	return city_map_path;
}

void CityMinimap::set_corner_size(int p_size) {
	corner_size = MAX(64, p_size);
}

int CityMinimap::get_corner_size() const {
	return corner_size;
}

void CityMinimap::set_corner_metres(float p_metres) {
	corner_metres = MAX(50.0f, p_metres);
}

float CityMinimap::get_corner_metres() const {
	return corner_metres;
}

// Opening the map shows all of it, centred on the player.
void CityMinimap::set_full_map(bool p_full) {
	if (p_full && !full_map) {
		view_zoom = 1.0f;
		view_middle = Vector2(resolution, resolution) * 0.5f;
		dragging = false;
	}
	full_map = p_full;
}

bool CityMinimap::get_full_map() const {
	return full_map;
}

} // namespace godot
