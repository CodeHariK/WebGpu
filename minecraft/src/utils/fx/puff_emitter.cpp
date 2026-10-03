#include "puff_emitter.h"

#include "toy_mesh.h"

#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

static const float POP_IN = 0.15f; // fraction of the life spent growing

void PuffEmitter::build(
		Node3D *p_owner,
		int p_count,
		float p_radius,
		const Color &p_color
) {
	Ref<SphereMesh> mesh = ToyMesh::sphere(p_radius, 8);
	Ref<StandardMaterial3D> mat = ToyMesh::toon(p_color);
	puffs.resize(p_count);
	velocity.assign(p_count, Vector3());
	age.assign(p_count, 0.0f);
	life.assign(p_count, 1.0f);
	size.assign(p_count, 1.0f);
	for (int i = 0; i < p_count; i++) {
		MeshInstance3D *p = ToyMesh::add(p_owner, mesh, mat, Vector3());
		p->set_as_top_level(true); // stays where it was emitted
		p->set_visible(false);
		puffs[i] = p;
	}
}

void PuffEmitter::emit(
		const Vector3 &p_position,
		const Vector3 &p_velocity,
		float p_life,
		float p_scale
) {
	if (puffs.empty()) {
		return;
	}
	int i = next;
	next = (next + 1) % (int)puffs.size();
	puffs[i]->set_global_position(p_position);
	puffs[i]->set_scale(Vector3(0.01f, 0.01f, 0.01f));
	puffs[i]->set_visible(true);
	velocity[i] = p_velocity;
	age[i] = 0.0f;
	life[i] = MAX(0.05f, p_life);
	size[i] = p_scale;
}

void PuffEmitter::burst(
		const Vector3 &p_center,
		int p_count,
		float p_speed,
		float p_life,
		float p_scale
) {
	for (int i = 0; i < p_count; i++) {
		float ang = Math::TAU * i / (float)p_count;
		Vector3 dir(std::cos(ang), 0.6f, std::sin(ang));
		emit(p_center, dir * p_speed, p_life, p_scale);
	}
}

void PuffEmitter::update(float p_dt) {
	for (size_t i = 0; i < puffs.size(); i++) {
		if (!puffs[i]->is_visible()) {
			continue;
		}
		age[i] += p_dt;
		float t = age[i] / life[i];
		if (t >= 1.0f) {
			puffs[i]->set_visible(false);
			continue;
		}
		// Quick grow, slow shrink: reads as a soft cartoon cloud.
		float s = (t < POP_IN) ? t / POP_IN : 1.0f - (t - POP_IN) / (1.0f - POP_IN);
		puffs[i]->set_scale(Vector3(s, s, s) * size[i]);
		puffs[i]->set_global_position(puffs[i]->get_global_position() + velocity[i] * p_dt);
		velocity[i] *= 1.0f - MIN(1.0f, 3.0f * p_dt); // drag
	}
}

} // namespace godot
