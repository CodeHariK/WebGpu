#ifndef PATROL_MELEE_ENEMY_H
#define PATROL_MELEE_ENEMY_H

#include "../../../ai/bt_store.h"
#include "../../../ai/bt_task.h"
#include "../../enemy_base.h"

namespace godot {

class PatrolMeleeEnemy : public EnemyBase {
	GDCLASS(PatrolMeleeEnemy, EnemyBase)

private:
	float patrol_speed = 2.0f;
	float approach_speed = 5.0f;
	float detection_range = 15.0f;
	float melee_range = 1.5f;

	Ref<BTTask> bt_root;
	Ref<BTStore> btstore;

protected:
	static void _bind_methods();

public:
	PatrolMeleeEnemy();
	~PatrolMeleeEnemy();

	void _physics_process(double delta) override;
	void melee_attack() override;
};

} // namespace godot

#endif // PATROL_MELEE_ENEMY_H
