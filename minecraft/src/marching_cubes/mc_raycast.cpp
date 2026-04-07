#include "mc_raycast.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event_mouse.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

MCRaycastHit raycast_from_mouse(Node3D *p_context, const Vector2 &p_mouse_pos, uint32_t p_mask, float p_dist) {
	MCRaycastHit hit;

	if (!p_context || !p_context->is_inside_tree()) {
		return hit;
	}

	Viewport *viewport = p_context->get_viewport();
	if (!viewport) {
		return hit;
	}

	Camera3D *camera = viewport->get_camera_3d();
	if (!camera) {
		return hit;
	}

	Vector3 from = camera->project_ray_origin(p_mouse_pos);
	Vector3 to = from + camera->project_ray_normal(p_mouse_pos) * p_dist;

	Ref<World3D> world = p_context->get_world_3d();
	if (world.is_null()) {
		return hit;
	}

	PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
	if (!space_state) {
		return hit;
	}

	Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(from, to, p_mask);
	Dictionary dc_hit = space_state->intersect_ray(query);

	if (!dc_hit.is_empty()) {
		hit.is_hit = true;
		hit.position = dc_hit["position"];
		hit.normal = dc_hit["normal"];
		hit.collider = Object::cast_to<Object>(dc_hit["collider"]);
		hit.rid = dc_hit["rid"];
		hit.shape = dc_hit["shape"];
	}

	return hit;
}

MCRaycastHit raycast_from_event(Node3D *p_context, const Ref<InputEvent> &p_event, uint32_t p_mask, float p_dist) {
	Ref<InputEventMouse> mouse_event = p_event;
	if (mouse_event.is_null()) {
		return MCRaycastHit();
	}

	return raycast_from_mouse(p_context, mouse_event->get_position(), p_mask, p_dist);
}

} // namespace godot
