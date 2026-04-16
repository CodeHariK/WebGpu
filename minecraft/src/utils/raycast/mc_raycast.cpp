#include "mc_raycast.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event_mouse.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

MCRaycastHit raycast_from_mouse(Node3D *p_context, const Vector2 &p_mouse_pos, uint32_t p_mask, float p_dist, const TypedArray<RID> &p_exclude) {
	if (!p_context || !p_context->is_inside_tree()) {
		return MCRaycastHit();
	}

	Viewport *viewport = p_context->get_viewport();
	if (!viewport) {
		return MCRaycastHit();
	}

	Camera3D *camera = viewport->get_camera_3d();
	if (!camera) {
		return MCRaycastHit();
	}

	Vector3 from = camera->project_ray_origin(p_mouse_pos);
	Vector3 to = from + camera->project_ray_normal(p_mouse_pos) * p_dist;

	return raycast_3d(p_context, from, to, p_mask, p_exclude);
}

MCRaycastHit raycast_3d(Node3D *p_context, const Vector3 &p_from, const Vector3 &p_to, uint32_t p_mask, const TypedArray<RID> &p_exclude) {
	MCRaycastHit hit;

	if (!p_context || !p_context->is_inside_tree()) {
		return hit;
	}

	Ref<World3D> world = p_context->get_world_3d();
	if (world.is_null()) {
		return hit;
	}

	PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
	if (!space_state) {
		return hit;
	}

	Ref<PhysicsRayQueryParameters3D> query = PhysicsRayQueryParameters3D::create(p_from, p_to, p_mask);
	if (p_exclude.size() > 0) {
		query->set_exclude(p_exclude);
	}
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

MCRaycastHit spherecast_3d(Node3D *p_context, const Vector3 &p_start, const Vector3 &p_direction, float p_dist, float p_radius, uint32_t p_mask, const TypedArray<RID> &p_exclude) {
	MCRaycastHit hit;

	if (!p_context || !p_context->is_inside_tree()) {
		return hit;
	}

	Ref<World3D> world = p_context->get_world_3d();
	if (world.is_null()) {
		return hit;
	}

	PhysicsDirectSpaceState3D *space_state = world->get_direct_space_state();
	if (!space_state) {
		return hit;
	}

	Ref<SphereShape3D> sphere = memnew(SphereShape3D);
	sphere->set_radius(p_radius);

	Ref<PhysicsShapeQueryParameters3D> params = memnew(PhysicsShapeQueryParameters3D);
	params->set_shape(sphere);
	params->set_transform(Transform3D(Basis(), p_start));
	params->set_motion(p_direction * p_dist);
	params->set_collision_mask(p_mask);
	
	if (p_exclude.size() > 0) {
		params->set_exclude(p_exclude);
	}
	
	// Use small margin for stability
	params->set_margin(0.01f);

	PackedFloat32Array cast_result = space_state->cast_motion(params);
	
	if (cast_result.size() >= 2 && cast_result[0] < 1.0f) {
		// Hit something
		hit.is_hit = true;
		
		// Move shape to contact point for rest info querying
		Vector3 contact_point = p_start + p_direction * (p_dist * cast_result[0]);
		params->set_transform(Transform3D(Basis(), contact_point));
		
		Dictionary rest_info = space_state->get_rest_info(params);
		if (!rest_info.is_empty()) {
			hit.position = rest_info["point"];
			hit.normal = rest_info["normal"];
			hit.collider = Object::cast_to<Object>(rest_info.get("collider_id", nullptr));
			hit.rid = rest_info.get("rid", RID());
			hit.shape = rest_info.get("shape", -1);
		} else {
			// Fallback if rest info fails
			hit.position = contact_point;
			hit.normal = -p_direction;
		}
	}

	return hit;
}

MCRaycastHit raycast_from_event(Node3D *p_context, const Ref<InputEvent> &p_event, uint32_t p_mask, float p_dist, const TypedArray<RID> &p_exclude) {
	Ref<InputEventMouse> mouse_event = p_event;
	if (mouse_event.is_null()) {
		return MCRaycastHit();
	}

	return raycast_from_mouse(p_context, mouse_event->get_position(), p_mask, p_dist, p_exclude);
}

} // namespace godot
