#ifndef TURRET_ENEMY_H
#define TURRET_ENEMY_H

#include "../../../ai/bt_store.h"
#include "../../../ai/bt_task.h"
#include "../../enemy_base.h"

namespace godot {
class ProjectileLauncher;
}

namespace godot {

class TurretEnemy : public EnemyBase {
	GDCLASS(TurretEnemy,
			EnemyBase)

private:
	float shoot_interval = 2.0f;
	float detection_range = 25.0f;

	Ref<BTTask> bt_root;
	Ref<BTStore> btstore;
	ProjectileLauncher *launcher = nullptr; ///< Child launcher (created at the top if missing).

protected:
	static void _bind_methods();

public:
	TurretEnemy();
	~TurretEnemy();

	void _ready() override;
	void _physics_process(double delta) override;
	void shoot() override;
};

} // namespace godot

#endif
