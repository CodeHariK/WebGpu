#ifndef TURRET_ENEMY_H
#define TURRET_ENEMY_H

#include "../../enemy_base.h"

namespace godot {

class TurretEnemy : public EnemyBase {
	GDCLASS(TurretEnemy, EnemyBase)

private:
	float shoot_interval = 2.0f;
	float shoot_timer = 0.0f;
	float detection_range = 25.0f;

protected:
	static void _bind_methods();

public:
	TurretEnemy();
	~TurretEnemy();

	void _physics_process(double delta) override;
	void shoot();
};

} // namespace godot

#endif
