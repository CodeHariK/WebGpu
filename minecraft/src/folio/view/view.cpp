#include "view.h"

#include "../ticker.h"

#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>

namespace godot {

FolioView::FolioView() {}

FolioView::~FolioView() {}

double FolioView::_aspect() const {
	const Vector2 size = get_viewport()->get_visible_rect().size;
	return (size.y > 0.0) ? ((double)size.x / (double)size.y) : ideal_ratio;
}

void FolioView::_ready() {
	// Active camera child (folio: 25deg vertical fov, near 0.1, far 200).
	camera = memnew(Camera3D);
	camera->set_fov((float)fov_degrees);
	camera->set_near(0.1f);
	camera->set_far(200.0f);
	add_child(camera);
	camera->make_current();

	// Init parts (folio seeds focus from the default respawn; use origin for now).
	spherical.init(quality_level, zoom.get_smoothed_ratio());
	focus_point.init(Vector3(0, 0, 0));

	const double aspect = _aspect();
	ratio_overflow = MAX(1.0, ideal_ratio / aspect) - 1.0;
	optimal_area.recompute(
			spherical.phi, spherical.theta, spherical.get_radius_max(ratio_overflow), Math::deg_to_rad(fov_degrees),
			aspect
	);

	// React to window resizes (stands in for FolioViewport's change events).
	Window *window = get_window();
	if (window) {
		window->connect("size_changed", callable_mp(this, &FolioView::_on_window_resized));
	}

	// Subscribe to the tick at priority 7 (folio's FolioView update order).
	if (!_subscribe()) {
		call_deferred("_subscribe");
	}

	update();
}

bool FolioView::_subscribe() {
	if (subscribed) {
		return true;
	}
	FolioTicker *ticker = FolioTicker::get_singleton();
	if (!ticker) {
		return false;
	}
	ticker->connect_tick(callable_mp(this, &FolioView::update), 7);
	subscribed = true;
	return true;
}

void FolioView::_on_window_resized() { resize(); }

void FolioView::update() {
	FolioTicker *ticker = FolioTicker::get_singleton();
	const double delta = ticker ? ticker->get_delta() : 1.0 / 60.0;
	const double delta_scaled = ticker ? ticker->get_delta_scaled() : delta;

	// 1. Focus point (returns its travel speed for the zoom).
	const double focus_speed = focus_point.update(delta);

	// 2. Zoom (default mode only).
	if (mode == MODE_DEFAULT) {
		zoom.update(delta, focus_speed, focus_point.get_is_tracking(), quality_level);
	}

	// 3. Spherical offset from the zoom ratio.
	spherical.update(zoom.get_smoothed_ratio(), ratio_overflow);

	// 4. Camera position = focus + orbit offset.
	position = focus_point.get_smoothed_position() + spherical.offset;
	view_delta = position - previous_position;
	previous_position = position;

	// 5. Look at the focus, then apply the roll (spring, scaled delta).
	Transform3D cam_xform =
			Transform3D(Basis(), position).looking_at(focus_point.get_smoothed_position(), Vector3(0, 1, 0));
	roll.update(delta_scaled);
	cam_xform.basis = cam_xform.basis * Basis(Quaternion(Vector3(0, 0, 1), roll.value));

	// 6. Cinematic blend (no-op unless a cinematic is in progress).
	cinematic.apply(cam_xform);

	// 7. Apply to the active camera.
	if (camera) {
		camera->set_global_transform(cam_xform);
	}

	// 8. Optimal area: recompute if needed, then slide to the focus.
	if (optimal_area.needs_update) {
		optimal_area.recompute(
				spherical.phi, spherical.theta, spherical.get_radius_max(ratio_overflow), Math::deg_to_rad(fov_degrees),
				_aspect()
		);
	}
	optimal_area.apply_focus(focus_point.get_smoothed_position(), focus_point.get_position());
}

void FolioView::resize() {
	ratio_overflow = MAX(1.0, ideal_ratio / _aspect()) - 1.0;
	optimal_area.needs_update = true; // reframe next update (folio: throttleResize)
}

void FolioView::set_target_position(const Vector3 &p_pos) { focus_point.set_tracked_position(p_pos); }

void FolioView::cinematic_start(
		const Vector3 &p_pos,
		const Vector3 &p_tgt
) {
	cinematic.start(p_pos, p_tgt, ratio_overflow);
}

void FolioView::cinematic_end() { cinematic.end(); }

void FolioView::set_cinematic_progress(double p_progress) { cinematic.set_progress(p_progress); }

void FolioView::roll_kick(double p_strength) { roll.kick(p_strength); }

void FolioView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioView::update);
	ClassDB::bind_method(D_METHOD("resize"), &FolioView::resize);
	ClassDB::bind_method(D_METHOD("_subscribe"), &FolioView::_subscribe);
	ClassDB::bind_method(D_METHOD("set_target_position", "pos"), &FolioView::set_target_position);
	ClassDB::bind_method(D_METHOD("set_quality_level", "level"), &FolioView::set_quality_level);
	ClassDB::bind_method(D_METHOD("cinematic_start", "pos", "target"), &FolioView::cinematic_start);
	ClassDB::bind_method(D_METHOD("cinematic_end"), &FolioView::cinematic_end);
	ClassDB::bind_method(D_METHOD("set_cinematic_progress", "progress"), &FolioView::set_cinematic_progress);
	ClassDB::bind_method(D_METHOD("roll_kick", "strength"), &FolioView::roll_kick);
	ClassDB::bind_method(D_METHOD("get_position"), &FolioView::get_position);
	ClassDB::bind_method(D_METHOD("get_optimal_radius"), &FolioView::get_optimal_radius);

	BIND_ENUM_CONSTANT(MODE_DEFAULT);
	BIND_ENUM_CONSTANT(MODE_FREE);
}

} // namespace godot
