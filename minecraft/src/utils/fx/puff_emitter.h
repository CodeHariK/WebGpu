#ifndef PUFF_EMITTER_H
#define PUFF_EMITTER_H

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>

#include <vector>

namespace godot {

/**
 * PuffEmitter — cartoon smoke puffs from a small recycled pool of spheres.
 * -----------------------------------------------------------------------
 * Each puff pops in fast, shrinks away slowly, drifts with its velocity and slows with
 * drag. The pool is a ring buffer of top-level MeshInstance3Ds parented to the owner, so
 * puffs stay where they were emitted and are freed with the owner. No allocation per puff.
 * Used for missile trails, crash smoke and pop bursts.
 */
class PuffEmitter {
public:
	void build(
			Node3D *p_owner,
			int p_count,
			float p_radius,
			const Color &p_color
	);
	/// p_scale: size multiplier for this puff (1 = p_radius from build).
	void emit(
			const Vector3 &p_position,
			const Vector3 &p_velocity,
			float p_life,
			float p_scale = 1.0f
	);
	/// A ring of `p_count` puffs flying outward (pop / explosion).
	void burst(
			const Vector3 &p_center,
			int p_count,
			float p_speed,
			float p_life,
			float p_scale = 1.0f
	);
	void update(float p_dt);
	bool is_built() const { return !puffs.empty(); }

private:
	std::vector<MeshInstance3D *> puffs;
	std::vector<Vector3> velocity;
	std::vector<float> age;
	std::vector<float> life;
	std::vector<float> size;
	int next = 0;
};

} // namespace godot

#endif // PUFF_EMITTER_H
