#ifndef FOLIO_MAP_H
#define FOLIO_MAP_H

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * Folio port — FolioMap  (folio `Game/Map.js`)
 * -----------------------------------------------
 * A top-down minimap overlay, toggled with M (or Esc to close). Folio's is a DOM
 * modal showing a prebaked day/night map image with clickable location pins and a
 * player dot that rotates with the car heading; this is the reusable Godot version:
 *
 *   - a dimmed full-screen overlay with a centred square board,
 *   - the terrain data texture (island shape) as the map background,
 *   - data-driven point pins (add_point / clear_points) — content-agnostic; the
 *     scene supplies its own points of interest,
 *   - a triangular player marker projected from the active target's world position
 *     and rotated to its heading (folio worldToMap: x,z / terrain.size + 0.5).
 *
 * Clicking a pin teleports the active vehicle to that point (folio respawns there).
 * Lives under the FolioUI root; created in FolioUI's boot next to the menu/modal.
 */
class FolioMap : public Control {
	GDCLASS(FolioMap,
			Control)

private:
	struct Point {
		String name;
		Vector3 position;
	};

	ColorRect *dim = nullptr; // full-screen dark scrim
	Panel *board = nullptr; // centred square map board
	TextureRect *map_rect = nullptr; // baked folio map image (day/night)
	Control *pins = nullptr; // parent for pin buttons
	Sprite2D *player_marker = nullptr; // "you are here" icon (folio player.png)

	Ref<Texture2D> map_day;
	Ref<Texture2D> map_night;
	bool showing_night = false;

	Vector<Point> points;
	bool open = false;
	bool ui_prev_visible = true; // restore FolioUI layer visibility on close
	double board_size = 560.0; // board edge length in px

	void _build();
	void _rebuild_pins();
	void _on_pin_pressed(int p_index);
	Vector2 _world_to_board(const Vector3 &p_world) const;
	double _terrain_size() const;
	Node *_active_target() const;

protected:
	static void _bind_methods();

public:
	FolioMap();
	~FolioMap();

	void _ready() override;
	void _process(double p_delta) override;
	void _unhandled_key_input(const Ref<InputEvent> &p_event) override;

	// Data-driven points of interest (world positions). The scene fills these.
	void add_point(const String &p_name, const Vector3 &p_world);
	void clear_points();

	void set_open(bool p_open);
	bool is_open() const { return open; }
	void toggle();
};

} // namespace godot

#endif // FOLIO_MAP_H
