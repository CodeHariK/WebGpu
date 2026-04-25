#ifndef ENEMY_MANAGER_H
#define ENEMY_MANAGER_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <vector>
#include <map>

namespace godot {

struct EnemyData {
	Node3D *node = nullptr;
	String kind;
	bool is_stunned = false;
	bool is_target_of_attack = false;
};

class EnemyManager : public Object {
	GDCLASS(EnemyManager, Object)

private:
	static EnemyManager *singleton;
	std::vector<EnemyData> enemies;

protected:
	static void _bind_methods();

public:
	EnemyManager();
	~EnemyManager();

	static EnemyManager *get_singleton();

	void register_enemy(Node3D *p_node, const String &p_kind);
	void unregister_enemy(Node3D *p_node);

	Node3D *get_best_target(Vector3 p_origin, Vector3 p_input_dir, float p_max_range);
	
	const std::vector<EnemyData>& get_enemies() const { return enemies; }
};

} // namespace godot

#endif // ENEMY_MANAGER_H
