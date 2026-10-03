#ifndef CELESTE_COMBAT_STATES_H
#define CELESTE_COMBAT_STATES_H

#include "../celeste_state.h"

#include <cstdint>

namespace godot {

class Node3D;

/**
 * CelesteAttackState — the hit button (C) with no dive target: an in-place spin attack.
 * ------------------------------------------------------------------------------------
 * Ground: plants the feet (horizontal speed brakes to a stop) and spins once.
 * Air:    a small upward pop + floaty fall (once per airtime, like Mario's spin), then spins.
 * Shortly into the spin, every enemy within `melee_range` takes a hit. The same button press
 * opens the parry window (CelesteController::is_parrying), so the spin also bats projectiles.
 */
class CelesteAttackState : public CelesteState {
	float time = 0.0f;
	bool hit_done = false;
	bool airborne = false;

public:
	using CelesteState::CelesteState;
	String get_name() const override { return "Attack"; }

	void enter() override;
	void exit() override;
	void physics_update(float delta) override;
};

/**
 * CelesteDiveKickState — air + C with an enemy in reach: a homing dive kick.
 * -------------------------------------------------------------------------
 *   Wind-up: a brief hang, tucked forward (the "tell").
 *   Dive:    fast, straight at the target (re-aimed every tick, so it can't be sidestepped
 *            by a slow enemy), feet first: a flying dropkick, or a stomp from straight above.
 *   Contact: the enemy takes a hit, an impact ring pops, and Celeste bounces up off it with
 *            air spin / double jump / dash refilled, so dive -> bounce -> dive chains.
 *   Miss:    target gone, ground reached, or too long -> falls normally.
 * The target is picked by CelesteController::_find_dive_target before entering.
 */
class CelesteDiveKickState : public CelesteState {
	uint64_t target_id = 0;
	float time = 0.0f;

	Node3D *_target() const;
	void _bounce(
			Node3D *p_target,
			const Vector3 &p_dir
	);

public:
	using CelesteState::CelesteState;
	String get_name() const override { return "DiveKick"; }

	void enter() override;
	void exit() override;
	void physics_update(float delta) override;
};

} // namespace godot

#endif // CELESTE_COMBAT_STATES_H
