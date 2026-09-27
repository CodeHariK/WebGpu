#include "map.h"

#include "../game.h"
#include "../terrain.h"
#include "ui.h"
#include "game_manager/game_manager.h"

#include "../cycles/day_cycles.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

using namespace godot;

FolioMap::FolioMap() {}
FolioMap::~FolioMap() {}

void FolioMap::_ready() {
	set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	set_mouse_filter(Control::MOUSE_FILTER_STOP); // eat clicks behind the map when open
	_build();
	set_visible(false);
	set_process(true);
	set_process_unhandled_key_input(true);
}

void FolioMap::_build() {
	// Full-screen dark scrim.
	dim = memnew(ColorRect);
	dim->set_name("Dim");
	dim->set_color(Color(0.02f, 0.02f, 0.04f, 0.72f));
	dim->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	dim->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(dim);

	// Centred square board.
	board = memnew(Panel);
	board->set_name("Board");
	board->set_size(Vector2(board_size, board_size));
	board->set_anchors_preset(Control::PRESET_CENTER);
	board->set_position(Vector2(-board_size * 0.5, -board_size * 0.5));
	add_child(board);

	// Baked folio map art (day/night), same top-down island render folio uses.
	ResourceLoader *rl = ResourceLoader::get_singleton();
	map_day = rl->load("res://assets/folio/map/map-day.png");
	map_night = rl->load("res://assets/folio/map/map-night.png");
	map_rect = memnew(TextureRect);
	map_rect->set_name("MapImage");
	map_rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	map_rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE); // fit the board, not native 1024px
	map_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	map_rect->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	if (map_day.is_valid()) {
		map_rect->set_texture(map_day);
	} else {
		// Fallback: raw terrain data texture if the art is missing.
		FolioGame *g = FolioGame::get_singleton();
		if (g && g->get_terrain()) {
			map_rect->set_texture(g->get_terrain()->get_terrain_data());
		}
	}
	board->add_child(map_rect);

	// Pins layer (buttons get their own clicks).
	pins = memnew(Control);
	pins->set_name("Pins");
	pins->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	pins->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	board->add_child(pins);

	// "You are here" icon (folio player.png), centred so it rotates about itself.
	player_marker = memnew(Sprite2D);
	player_marker->set_name("Player");
	Ref<Texture2D> player_tex = rl->load("res://assets/folio/map/player.png");
	if (player_tex.is_valid()) {
		player_marker->set_texture(player_tex);
		float h = MAX((float)player_tex->get_height(), 1.0f);
		float s = 26.0f / h; // scale the icon down to ~26px tall on the board
		player_marker->set_scale(Vector2(s, s));
	}
	board->add_child(player_marker);
}

double FolioMap::_terrain_size() const {
	FolioGame *g = FolioGame::get_singleton();
	if (g && g->get_terrain()) {
		return g->get_terrain()->get_size();
	}
	return 192.0;
}

Node *FolioMap::_active_target() const {
	GameManager *gm = GameManager::get_singleton();
	return gm ? gm->get_active_target() : nullptr;
}

Vector2 FolioMap::_world_to_board(const Vector3 &p_world) const {
	double size = MAX(_terrain_size(), 0.001);
	double u = CLAMP(p_world.x / size + 0.5, 0.0, 1.0);
	double v = CLAMP(p_world.z / size + 0.5, 0.0, 1.0);
	return Vector2((float)(u * board_size), (float)(v * board_size));
}

void FolioMap::add_point(const String &p_name, const Vector3 &p_world) {
	Point pt;
	pt.name = p_name;
	pt.position = p_world;
	points.push_back(pt);
	if (pins) {
		_rebuild_pins();
	}
}

void FolioMap::clear_points() {
	points.clear();
	_rebuild_pins();
}

void FolioMap::_rebuild_pins() {
	if (!pins) {
		return;
	}
	// Clear old pin buttons.
	TypedArray<Node> kids = pins->get_children();
	for (int i = 0; i < kids.size(); i++) {
		Node *n = Object::cast_to<Node>(kids[i]);
		if (n) {
			n->queue_free();
		}
	}
	// One flat button per point; click teleports the active vehicle there.
	for (int i = 0; i < points.size(); i++) {
		Button *b = memnew(Button);
		b->set_text(points[i].name);
		b->set_flat(true);
		b->set_size(Vector2(140, 24));
		Vector2 px = _world_to_board(points[i].position);
		b->set_position(px - Vector2(70, 12));
		b->set_mouse_filter(Control::MOUSE_FILTER_STOP);
		pins->add_child(b);
		b->connect("pressed", callable_mp(this, &FolioMap::_on_pin_pressed).bind(i));
	}
}

void FolioMap::_on_pin_pressed(int p_index) {
	if (p_index < 0 || p_index >= points.size()) {
		return;
	}
	Node3D *n = Object::cast_to<Node3D>(_active_target());
	if (n) {
		n->set_global_position(points[p_index].position + Vector3(0, 1.0f, 0));
		RigidBody3D *rb = Object::cast_to<RigidBody3D>(n);
		if (rb) {
			rb->set_linear_velocity(Vector3());
			rb->set_angular_velocity(Vector3());
		}
	}
	set_open(false);
}

void FolioMap::_process(double p_delta) {
	if (!open || !player_marker) {
		return;
	}
	Node3D *n = Object::cast_to<Node3D>(_active_target());
	if (!n) {
		player_marker->set_visible(false);
		return;
	}
	player_marker->set_visible(true);
	Vector3 pos = n->get_global_position();
	player_marker->set_position(_world_to_board(pos));
	// Heading: car forward is -Z; project onto the map (x right, z down).
	Vector3 fwd = -n->get_global_transform().basis.get_column(2);
	const float HALF_PI = 1.57079632679f;
	player_marker->set_rotation(Math::atan2(fwd.z, fwd.x) - HALF_PI);

	// Swap the baked day/night map art from the day cycle (day/dusk/night/dawn loop).
	FolioGame *g = FolioGame::get_singleton();
	if (g && g->get_day_cycles() && map_day.is_valid() && map_night.is_valid()) {
		double prog = g->get_day_cycles()->get_progress();
		bool night = (prog > 0.35 && prog < 0.7);
		if (night != showing_night) {
			showing_night = night;
			map_rect->set_texture(night ? map_night : map_day);
		}
	}
}

void FolioMap::set_open(bool p_open) {
	open = p_open;
	// The FolioUI layer may be hidden (e.g. the car_world driving test hides the
	// whole HUD). Force it visible while the map is open so the overlay renders,
	// then restore whatever it was on close.
	FolioUI *ui = FolioUI::get_singleton();
	if (p_open) {
		if (ui) {
			ui_prev_visible = ui->is_visible();
			ui->set_visible(true);
		}
		set_visible(true);
		_rebuild_pins();
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	} else {
		set_visible(false);
		if (ui && !ui_prev_visible) {
			ui->set_visible(false);
		}
	}
}

void FolioMap::toggle() {
	set_open(!open);
}

void FolioMap::_unhandled_key_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;
	if (k.is_null() || !k->is_pressed() || k->is_echo()) {
		return;
	}
	if (k->get_physical_keycode() == KEY_M) {
		toggle();
		accept_event();
	} else if (open && k->get_keycode() == KEY_ESCAPE) {
		set_open(false);
		accept_event();
	}
}

void FolioMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_point", "name", "world"), &FolioMap::add_point);
	ClassDB::bind_method(D_METHOD("clear_points"), &FolioMap::clear_points);
	ClassDB::bind_method(D_METHOD("set_open", "open"), &FolioMap::set_open);
	ClassDB::bind_method(D_METHOD("is_open"), &FolioMap::is_open);
	ClassDB::bind_method(D_METHOD("toggle"), &FolioMap::toggle);
}
