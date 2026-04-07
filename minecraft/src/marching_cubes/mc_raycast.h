#ifndef MC_RAYCAST_H
#define MC_RAYCAST_H

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/rid.hpp>

namespace godot {

/**
 * Strongly-typed hit result for a 3D raycast.
 */
struct MCRaycastHit {
	bool is_hit = false;
	Vector3 position;
	Vector3 normal;
	Object *collider = nullptr;
	RID rid;
	int shape = -1;
};

/**
 * Generic raycast helper to perform a 3D raycast from a 2D screen/mouse position.
 * Returns a typed MCRaycastHit struct.
 */
MCRaycastHit raycast_from_mouse(Node3D *p_context, const Vector2 &p_mouse_pos, uint32_t p_mask = 0xFFFFFFFF, float p_dist = 1000.0f);

/**
 * Helper to perform a 3D raycast directly from a mouse InputEvent.
 * Returns a typed MCRaycastHit struct.
 */
MCRaycastHit raycast_from_event(Node3D *p_context, const Ref<InputEvent> &p_event, uint32_t p_mask = 0xFFFFFFFF, float p_dist = 1000.0f);

} // namespace godot

#endif // MC_RAYCAST_H
