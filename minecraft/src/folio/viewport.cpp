#include "viewport.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

FolioViewport::FolioViewport() {}

FolioViewport::~FolioViewport() {}

void FolioViewport::_ready() {
	events.instantiate();

	// One-shot debounce timer for the "throttleChange" event.
	throttle_timer = memnew(Timer);
	throttle_timer->set_one_shot(true);
	throttle_timer->set_wait_time(throttle_duration);
	add_child(throttle_timer);
	throttle_timer->connect("timeout", callable_mp(this, &FolioViewport::_on_throttle_timeout));

	measure();

	// Mirror the DOM `resize` listener with the root window's size_changed signal.
	Window *window = get_window();
	if (window) {
		window->connect("size_changed", callable_mp(this, &FolioViewport::_on_resize));
	}
}

void FolioViewport::measure() {
	DisplayServer *ds = DisplayServer::get_singleton();
	if (ds) {
		const Vector2i size = ds->window_get_size();
		width = size.x;
		height = size.y;
		pixel_ratio_pure = ds->screen_get_scale();
	}

	ratio = (height > 0) ? ((double)width / (double)height) : 1.0;
	pixel_ratio = MIN(pixel_ratio_pure, pixel_ratio_max);
}

void FolioViewport::_on_resize() {
	measure();
	if (events.is_valid()) {
		events->trigger("change", Array());
	}

	// (Re)start the debounce; fires "throttleChange" once resizing settles.
	if (throttle_timer) {
		throttle_timer->start();
	}
}

void FolioViewport::_on_throttle_timeout() {
	if (events.is_valid()) {
		events->trigger("throttleChange", Array());
	}
}

void FolioViewport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("measure"), &FolioViewport::measure);
	ClassDB::bind_method(D_METHOD("get_width"), &FolioViewport::get_width);
	ClassDB::bind_method(D_METHOD("get_height"), &FolioViewport::get_height);
	ClassDB::bind_method(D_METHOD("get_ratio"), &FolioViewport::get_ratio);
	ClassDB::bind_method(D_METHOD("get_pixel_ratio"), &FolioViewport::get_pixel_ratio);
	ClassDB::bind_method(D_METHOD("get_size"), &FolioViewport::get_size);
	ClassDB::bind_method(D_METHOD("get_events"), &FolioViewport::get_events);
}

} // namespace godot
