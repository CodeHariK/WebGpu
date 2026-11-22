/**
 * @file city_minimap.h
 * @brief CityMinimap: a map of the CityMap city in the top left corner of the screen, turning with
 * nothing (north is the world's −z), centred on the player (GameManager's active target, a car or
 * a character) with an arrow for the way they face.
 *
 * M (or a tap on the corner map) opens the whole map in the middle of the screen, centred on the
 * player: drag to pan, the mouse wheel or a trackpad pinch to zoom (round the pointer); M, Escape
 * or the × in its corner closes it.
 *
 * The map is a picture of the city taken once from straight above (a SubViewport with an
 * orthographic camera over the whole map, rendered a few frames after the city is built), so it
 * costs nothing per frame but drawing a part of it.
 */
#ifndef CITY_MINIMAP_H
#define CITY_MINIMAP_H

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>

namespace godot {

class CityMap;

class CityMinimap : public Control {
	GDCLASS(CityMinimap,
			Control)

private:
	NodePath city_map_path;
	int corner_size = 240; // pixels: the minimap's side
	float corner_metres = 600.0f; // metres of the city it shows across
	int resolution = 2048; // pixels: the picture of the whole map
	bool full_map = false;

	SubViewport *viewport = nullptr;
	Camera3D *camera = nullptr;
	int frames_waited = 0;
	bool taken = false;
	Vector3 centre; // world: the map's middle, under the camera
	float map_size = 0.0f;

	// the open map: the picture point at its middle, and how far in (1: all of it)
	Vector2 view_middle;
	float view_zoom = 1.0f;
	bool dragging = false;

	CityMap *_city_map() const;
	const Node3D *_target() const;
	void _take_picture();
	void _place();
	Vector2 _to_picture(const Vector3 &p_world) const;
	Rect2 _shown() const; // the part of the picture the open map shows
	Rect2 _close_box() const;
	void _zoom_at(
			const Vector2 &p_at,
			float p_factor
	);
	void _pan_by(const Vector2 &p_screen_delta);
	void _draw_arrow(
			const Vector2 &p_at,
			const Node3D *p_target,
			float p_size
	);
	void _draw_open();
	void _draw_corner();

protected:
	static void _bind_methods();

public:
	void _ready() override;
	void _process(double p_delta) override;
	void _draw() override;
	void _gui_input(const Ref<InputEvent> &p_event) override;
	void _unhandled_input(const Ref<InputEvent> &p_event) override;

	void set_city_map_path(const NodePath &p_path);
	NodePath get_city_map_path() const;

	void set_corner_size(int p_size);
	int get_corner_size() const;

	void set_corner_metres(float p_metres);
	float get_corner_metres() const;

	void set_full_map(bool p_full);
	bool get_full_map() const;
};

} // namespace godot

#endif // CITY_MINIMAP_H
