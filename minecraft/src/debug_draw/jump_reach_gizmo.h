#ifndef JUMP_REACH_GIZMO_H
#define JUMP_REACH_GIZMO_H

#include "../character/jump_metrics.h"

#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

namespace godot {

class Label3D;
class Node3D;

/**
 * F3 debug view: the active character's single-jump reach on flat ground, as three
 * DebugManager rings around the last take-off spot (frozen while airborne, so you can
 * judge the jump you're in):
 *
 *   green = comfortable (70% of run reach)   yellow = precise (90%)   orange = run limit
 *   faint blue = double jump limit (best press timing), when the character has an air jump
 *   faint purple = jump + double jump + air dash limit (best order and timing)
 *
 * plus the run arc straight ahead and a label with the numbers (incl. sprint and a
 * ledge reach). Everything comes from JumpMetrics, so it follows the tuning sliders live.
 */
class JumpReachGizmo : public MeshInstance3D {
	GDCLASS(JumpReachGizmo,
			MeshInstance3D)

private:
	Ref<ImmediateMesh> mesh;
	Ref<StandardMaterial3D> material;
	Label3D *label = nullptr;
	Vector3 anchor;
	Vector3 forward = Vector3(0, 0, -1);
	bool has_anchor = false;

	void _update_anchor(Node3D *p_target);
	void _draw_rings(const JumpMetrics &p_m);
	void _draw_arc(const JumpMetrics &p_m);

protected:
	static void _bind_methods() {}

public:
	JumpReachGizmo();

	/// Rebuild for p_target (clears for nodes without jump metrics).
	void update_for(Node3D *p_target);
	/// Remove the rings and arc (the view was switched off).
	void clear();
};

} // namespace godot

#endif // JUMP_REACH_GIZMO_H
