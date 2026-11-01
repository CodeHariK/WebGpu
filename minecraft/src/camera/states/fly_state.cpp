#include "fly_state.h"
#include "../../game_manager/player_input.h"
#include "../../utils/raycast/mc_raycast.h"
#include "../camera.h"
#include <cmath>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

namespace {
const float ZOOM_FRACTION = 0.15f; ///< one wheel step moves this fraction of the distance to the point
const float CLOSEST = 0.15f; ///< zooming in stops this far from the surface
const float RAY_LENGTH = 2000.0f;
} // namespace

void CameraStateFly::enter(GameCamera *p_camera) {
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	// Seat the springs on wherever the camera currently is so entering fly mode
	// (or returning to it) never snaps.
	p_camera->rebase_springs();
	was_dragging = false;
}

bool CameraStateFly::point_under_mouse(
		GameCamera *p_camera,
		Vector3 &r_point
) const {
	Viewport *viewport = p_camera->get_viewport();
	Vector2 mouse = viewport ? viewport->get_mouse_position() : Vector2();
	MCRaycastHit hit = raycast_from_mouse(p_camera, mouse, p_camera->collision_mask, RAY_LENGTH);
	if (hit.is_hit) {
		r_point = hit.position;
		return true;
	}
	r_point = p_camera->project_ray_origin(mouse) + p_camera->project_ray_normal(mouse) * depth;
	return false;
}

// Turn round the pivot: keep the camera's offset from it fixed in camera space, so the pivot stays
// where it is on screen while the view turns.
void CameraStateFly::orbit(
		GameCamera *p_camera,
		const Vector2 &p_look
) {
	Basis before = Basis::from_euler(Vector3(p_camera->pitch, p_camera->yaw, 0));
	p_camera->yaw -= p_look.x * p_camera->orbit_sensitivity;
	p_camera->pitch -= p_look.y * p_camera->orbit_sensitivity;
	p_camera->pitch = CLAMP(p_camera->pitch, -Math::PI * 0.49f, Math::PI * 0.49f);
	Basis after = Basis::from_euler(Vector3(p_camera->pitch, p_camera->yaw, 0));
	Vector3 offset_in_view = before.xform_inv(p_camera->pos_spring.target - pivot);
	p_camera->pos_spring.target = pivot + after.xform(offset_in_view);
}

// Slide the camera so the grabbed point follows the mouse: one pixel is
// 2 * depth * tan(fov / 2) / viewport height metres at the pivot's depth.
void CameraStateFly::pan(
		GameCamera *p_camera,
		const Vector2 &p_look
) {
	Transform3D t = p_camera->get_global_transform();
	Vector3 forward = -t.basis.get_column(2);
	float along = Math::max((pivot - t.origin).dot(forward), 0.05f);
	Viewport *viewport = p_camera->get_viewport();
	float height = viewport ? Math::max(viewport->get_visible_rect().size.y, 1.0f) : 720.0f;
	float metres_per_pixel = 2.0f * along * std::tan(Math::deg_to_rad(p_camera->get_fov()) * 0.5f) / height;
	Vector3 move = t.basis.get_column(0) * (-p_look.x * metres_per_pixel) +
			t.basis.get_column(1) * (p_look.y * metres_per_pixel);
	p_camera->pos_spring.target += move;
	pivot += move;
}

// Move toward the point under the mouse by a fraction of the distance to it (away when zooming
// out), so the steps shrink as you get close; never closer than CLOSEST.
void CameraStateFly::zoom(
		GameCamera *p_camera,
		float p_steps
) {
	Vector3 target;
	bool hit = point_under_mouse(p_camera, target);
	Vector3 from = p_camera->pos_spring.target;
	float distance = (target - from).length();
	if (distance < 1e-4f) {
		return;
	}
	Vector3 dir = (target - from) / distance;
	float keep = std::pow(1.0f - ZOOM_FRACTION * p_camera->zoom_speed, p_steps); // < 1 in, > 1 out
	// zooming out from very close: at least 1 m's worth, so it doesn't crawl away from the surface
	float scale = p_steps < 0.0f ? Math::max(distance, 1.0f) : distance;
	float step = scale * (1.0f - keep);
	step = Math::min(step, distance - CLOSEST);
	p_camera->pos_spring.target = from + dir * step;
	if (hit) {
		depth = Math::max(distance - step, CLOSEST); // over the sky the depth stays, so zoom keeps its pace
	}
}

void CameraStateFly::update(
		GameCamera *p_camera,
		float p_delta
) {
	if (p_camera->get_player_input()) {
		const ActionState &state = p_camera->get_player_input()->get_state();
		bool dragging = state.camera.is_orbiting;
		if (dragging && !was_dragging) {
			// a drag starts: the point under the mouse is what we orbit / pan
			if (point_under_mouse(p_camera, pivot)) {
				depth = (pivot - p_camera->get_global_position()).length();
			}
		}
		was_dragging = dragging;
		if (dragging) {
			if (state.camera.is_panning) {
				pan(p_camera, state.camera.look_delta);
			} else {
				orbit(p_camera, state.camera.look_delta);
			}
		}
		if (std::abs(state.camera.zoom_delta) > 0.001f) {
			zoom(p_camera, state.camera.zoom_delta);
		}
	}

	// No smoothing, like Blender's viewport: an orbit has to keep its pivot exactly in place, which a
	// lagging rotation spring would break, so the springs are snapped onto their targets.
	(void)p_delta;
	p_camera->yaw = UtilityFunctions::wrapf(p_camera->yaw, -Math::PI, Math::PI);
	p_camera->yaw_spring.reset(p_camera->yaw);
	p_camera->pitch_spring.reset(p_camera->pitch);
	p_camera->pos_spring.reset(p_camera->pos_spring.target);
	p_camera->apply_position(p_camera->pos_spring.target, 0.0f);
}

} // namespace godot
