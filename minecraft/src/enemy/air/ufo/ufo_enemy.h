#ifndef UFO_ENEMY_H
#define UFO_ENEMY_H

#include "../flying_enemy.h"

#include <vector>

namespace godot {

class MeshInstance3D;

/**
 * UfoEnemy — a wobbly flying saucer piloted by a tiny alien. Hops over you and drops goo.
 * ---------------------------------------------------------------------------------------
 *   Hop:    picks a spot `hop_radius` around the player at `altitude` and glides there
 *           (keeps hopping without attacking until `fire_interval` has passed since the last drop).
 *   Hover:  stops, wobbles, beams down a green tractor-light cone (the tell), then drops a
 *           ballistic goo bomb (default: res://assets/projectiles/ufo_bomb.tres), whose
 *           landing zone is drawn on the ground. Then hops again.
 *   Idle:   drifts between random spots around where it was placed.
 *   Look:   flat saucer + glass dome + green alien, a ring of blinking lights that spins,
 *           a constant lazy tilt-wobble.
 */
class UfoEnemy : public FlyingEnemy {
	GDCLASS(UfoEnemy,
			FlyingEnemy)

private:
	float hop_radius = 6.0f;
	float hover_time = 1.4f;

	Vector3 goal;
	bool has_goal = false;
	float hover_t = -1.0f; ///< >=0: hovering at the goal (beam + drop)
	bool dropped = false;

	Node3D *saucer = nullptr; ///< spins slowly (the light ring goes round)
	MeshInstance3D *beam = nullptr;
	Ref<StandardMaterial3D> beam_mat;
	std::vector<Ref<StandardMaterial3D>> lights;

	void _pick_goal(
			const Vector3 &p_center,
			float p_radius
	);

protected:
	static void _bind_methods();

	void _build_model() override;
	void _think(
			float p_dt,
			Node3D *p_target
	) override;
	void _animate(float p_dt) override;
	String _default_profile_path() const override { return "res://assets/projectiles/ufo_bomb.tres"; }

public:
	UfoEnemy();

	void set_hop_radius(float p_v) { hop_radius = p_v; }
	float get_hop_radius() const { return hop_radius; }
	void set_hover_time(float p_v) { hover_time = p_v; }
	float get_hover_time() const { return hover_time; }
};

} // namespace godot

#endif // UFO_ENEMY_H
