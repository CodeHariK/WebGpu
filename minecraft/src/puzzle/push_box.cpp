#include "push_box.h"

#include "../game_manager/game_constants.h"
#include "puzzle_props.h"
#include "../utils/fx/toy_mesh.h"

#include <godot_cpp/classes/mesh_instance3d.hpp>

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>

namespace godot {

void PushBox::setup(
		float p_cell,
		bool p_bomb
) {
	bomb = p_bomb;
	// Synced to physics only while sliding (riders get carried smoothly); idle boxes stay
	// plain children, so moving the grid in the editor / at runtime carries them along.
	set_sync_to_physics(false);
	set_collision_layer(toLayer(LAYER_OBJECTS));
	set_collision_mask(0); // never pushed by physics

	const float s = p_cell * 0.9f;
	size = s;
	Ref<BoxShape3D> shape;
	shape.instantiate();
	shape->set_size(Vector3(s, s, s));
	CollisionShape3D *cs = memnew(CollisionShape3D);
	cs->set_shape(shape);
	cs->set_position(Vector3(0, s * 0.5f, 0));
	add_child(cs);

	if (bomb) {
		PuzzleProps::bomb_crate(this, p_cell);
	} else {
		PuzzleProps::crate(this, p_cell);
	}
}

void PushBox::slide_to(
		const Vector3 &p_local_target,
		float p_time
) {
	from = get_position();
	to = p_local_target;
	slide_time = MAX(p_time, 0.01f);
	slide_t = 0.0f;
	sliding = true;
	set_sync_to_physics(true);
}

void PushBox::_physics_process(double p_delta) {
	if (!sliding) {
		return;
	}
	slide_t = MIN(slide_t + (float)p_delta / slide_time, 1.0f);
	const float e = slide_t * slide_t * (3.0f - 2.0f * slide_t); // smoothstep
	set_position(from.lerp(to, e));
	if (slide_t >= 1.0f) {
		sliding = false;
		set_sync_to_physics(false);
	}
}

void PushBox::set_wet(bool p_wet) {
	Ref<Material> overlay = p_wet ? Ref<Material>(ToyMesh::glow(Color(0.05f, 0.15f, 0.25f, 0.35f))) : Ref<Material>();
	TypedArray<Node> meshes = find_children("*", "MeshInstance3D", true, false);
	for (int i = 0; i < meshes.size(); ++i) {
		MeshInstance3D *m = Object::cast_to<MeshInstance3D>(meshes[i]);
		if (m) {
			m->set_material_overlay(overlay);
		}
	}
}

} // namespace godot
