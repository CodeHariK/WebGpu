#ifndef ENEMY_BASE_H
#define ENEMY_BASE_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class EnemyBase : public CharacterBody3D {
	GDCLASS(EnemyBase, CharacterBody3D)

private:
	String enemy_kind = "Thug";

protected:
	static void _bind_methods();
	
	float health = 3.0f;
	float max_health = 3.0f;
	bool is_dead = false;

public:
	EnemyBase();
	~EnemyBase();

	void _ready() override;
	void _exit_tree() override;

	virtual void take_damage(float p_amount);
	virtual void die();
	virtual void shoot() {}

	void set_enemy_kind(const String &p_kind) { enemy_kind = p_kind; }
	String get_enemy_kind() const { return enemy_kind; }

	float get_health() const { return health; }
	bool get_is_dead() const { return is_dead; }
};

} // namespace godot

#endif // ENEMY_BASE_H
