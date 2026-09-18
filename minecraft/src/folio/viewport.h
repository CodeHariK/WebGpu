#ifndef FOLIO_VIEWPORT_H
#define FOLIO_VIEWPORT_H

#include "events.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

/**
 * Folio port — Viewport  (registered as `FolioViewport`)
 * ------------------------------------------------------
 * Faithful port of folio-2025 `Game/Viewport.js`: measures the drawable size and
 * device pixel ratio, and broadcasts resize events that render/layout systems
 * react to.
 *
 * FolioEvents (on the owned `FolioEvents` bus, names kept 1:1 with folio):
 *   - "change"        fired immediately on every resize (Rendering resizes on it).
 *   - "throttleChange" fired once, 400 ms after resizing stops (heavy rebuilds,
 *                      e.g. Floor regenerates its plane on it).
 *
 * Web -> Godot mapping:
 *   - DOM `getBoundingClientRect()` size -> `DisplayServer::window_get_size()`.
 *   - `window.devicePixelRatio`          -> `DisplayServer::screen_get_scale()`,
 *                                           clamped to `pixel_ratio_max` (2).
 *   - the `resize` DOM listener          -> the root `Window::size_changed` signal.
 *
 * Renamed to `FolioViewport` because Godot already has a core `Viewport` class.
 */
class FolioViewport : public Node {
	GDCLASS(FolioViewport,
			Node)

private:
	int width = 0;
	int height = 0;
	double ratio = 1.0;

	double pixel_ratio_pure = 1.0;
	double pixel_ratio_max = 2.0;
	double pixel_ratio = 1.0;

	double throttle_duration = 0.4; // seconds (folio: 400 ms)
	Timer *throttle_timer = nullptr;

	Ref<FolioEvents> events;

	void _on_resize();
	void _on_throttle_timeout();

protected:
	static void _bind_methods();

public:
	FolioViewport();
	~FolioViewport();

	void _ready() override;

	// Re-read size + pixel ratio from the display.
	void measure();

	int get_width() const { return width; }
	int get_height() const { return height; }
	double get_ratio() const { return ratio; }
	double get_pixel_ratio() const { return pixel_ratio; }
	Vector2i get_size() const { return Vector2i(width, height); }

	Ref<FolioEvents> get_events() const { return events; }
};

} // namespace godot

#endif // FOLIO_VIEWPORT_H
